#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
package.py - 将整个程序打包为单个可执行文件（自解压启动器）

用法:
    python tools\\package.py                 # 构建 + 部署 + 打包
    python tools\\package.py --no-build      # 跳过编译/部署，直接用现有 build\\bin 打包
    python tools\\package.py --no-auto-install  # 禁止自动下载 Qt 工具链

产物:
    dist\\闪词.exe      单文件，双击即启动系统

工具链:
    Qt / MinGW / Ninja 会自动探测（C:\\Qt、%USERPROFILE%\\Qt、%LOCALAPPDATA%\\Qt，
    或 PATH 上的 qmake / g++ / ninja）。
    若缺失且允许联网，会自动用 aqtinstall 安装（约 1GB）。
    可用环境变量 QT_DIR / MINGW_DIR / NINJA_DIR 手动指定。

原理:
    Bootstrapper.c 编译出一个不依赖 Qt/C++ 运行库的启动器，
    将 build\\bin 下的全部运行时文件（Qt DLL、插件、内置词表等）以 zlib 压缩后
    作为 RCDATA 资源嵌入其中。运行时会解压到
    %LOCALAPPDATA%\\ShanCi\\app 并启动 ShanCi.exe。
    用户数据（data\\vocab.db 等）不在压缩包内，升级时不会被覆盖。
"""

import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib

# --- 工具链版本（仅自动安装时使用） ---
QT_VERSION = "6.8.3"
MINGW_TOOL = "tools_mingw1310"

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JUNCTION = r"C:\shanci"

OUTPUT_NAME = "闪词.exe"

# 打包时排除的用户数据与开发产物（升级安装时不应覆盖，也不应随包分发）
EXCLUDE_PREFIXES = (
    "data/vocab.db",
    "data/vocab.log",
    "data/config.json",
    "data/audio/",
    "data/sync/",
    "test_",
)

MAGIC = b"VTPK"
FMT_VERSION = 1
COMPRESS_LEVEL = 6


def log(msg):
    print("[package] " + msg, flush=True)


def run(cmd, cwd=None, env=None):
    log("执行: " + (" ".join(cmd) if isinstance(cmd, list) else cmd))
    rc = subprocess.call(cmd, cwd=cwd, env=env)
    if rc != 0:
        raise SystemExit("命令失败（退出码 %d）: %s" % (rc, cmd))


# ---------------------------------------------------------------------------
# 工具链探测 / 自动安装
# ---------------------------------------------------------------------------
def _has(path, *parts):
    return bool(path) and os.path.isfile(os.path.join(path, *parts))


def _newest_first(names):
    def key(n):
        nums = re.findall(r"\d+", n)
        return [int(x) for x in nums[:3]] if nums else [0]

    return sorted(names, key=key, reverse=True)


def _search_roots():
    roots = [r"C:\Qt"]
    if os.environ.get("USERPROFILE"):
        roots.append(os.path.join(os.environ["USERPROFILE"], "Qt"))
    if os.environ.get("LOCALAPPDATA"):
        roots.append(os.path.join(os.environ["LOCALAPPDATA"], "Qt"))
    return [r for r in roots if os.path.isdir(r)]


def find_qt():
    cands = []
    if os.environ.get("QT_DIR"):
        cands.append(os.environ["QT_DIR"])
    for root in _search_roots():
        vers = [d for d in os.listdir(root)
                if os.path.isdir(os.path.join(root, d)) and re.match(r"^\d+\.\d+", d)]
        for v in _newest_first(vers):
            for arch in ("mingw_64", "msvc2022_64", "msvc2019_64"):
                cands.append(os.path.join(root, v, arch))
    for exe in ("qmake6", "qmake"):
        p = shutil.which(exe)
        if p:
            cands.append(os.path.dirname(os.path.dirname(p)))
            break
    for c in cands:
        if _has(c, "lib", "cmake", "Qt6", "Qt6Config.cmake"):
            return os.path.abspath(c)
    return None


def find_mingw(qt_dir):
    cands = []
    if os.environ.get("MINGW_DIR"):
        cands.append(os.environ["MINGW_DIR"])
    if qt_dir:
        tools = os.path.join(os.path.dirname(os.path.dirname(qt_dir)), "Tools")
        if os.path.isdir(tools):
            subs = [d for d in os.listdir(tools) if d.lower().startswith("mingw")]
            for d in _newest_first(subs):
                cands.append(os.path.join(tools, d))
    cands += [r"C:\mingw64", r"D:\mingw64"]
    g = shutil.which("g++")
    if g:
        cands.append(os.path.dirname(os.path.dirname(g)))
    for c in cands:
        if _has(c, "bin", "g++.exe"):
            return os.path.abspath(c)
    return None


def find_ninja(qt_dir):
    cands = []
    if os.environ.get("NINJA_DIR"):
        cands.append(os.environ["NINJA_DIR"])
    if qt_dir:
        cands.append(os.path.join(os.path.dirname(os.path.dirname(qt_dir)), "Tools", "Ninja"))
    cands.append(r"C:\Qt\Tools\Ninja")
    n = shutil.which("ninja")
    if n:
        cands.append(os.path.dirname(n))
    for c in cands:
        if _has(c, "ninja.exe"):
            return os.path.abspath(c)
    return None


def install_toolchain():
    root = os.environ.get("QT_INSTALL_ROOT") or r"C:\Qt"
    try:
        os.makedirs(root, exist_ok=True)
    except OSError:
        root = os.path.join(os.environ.get("LOCALAPPDATA") or os.path.expanduser("~"), "Qt")
        os.makedirs(root, exist_ok=True)

    log("未找到 Qt 工具链，正在自动安装 Qt %s + MinGW + Ninja 到 %s（约 1GB，请稍候）..."
        % (QT_VERSION, root))
    py = sys.executable
    run([py, "-m", "pip", "install", "--quiet", "--upgrade", "aqtinstall"])
    run([py, "-m", "aqt", "install-qt", "windows", "desktop", QT_VERSION, "win64_mingw", "-O", root])
    subprocess.call([py, "-m", "aqt", "install-qt", "windows", "desktop", QT_VERSION, "win64_mingw",
                     "-m", "qtmultimedia", "-O", root])
    subprocess.call([py, "-m", "aqt", "install-tool", "windows", "desktop", MINGW_TOOL, "-O", root])
    subprocess.call([py, "-m", "aqt", "install-tool", "windows", "desktop", "tools_ninja", "-O", root])
    log("工具链安装完成。")


def resolve_toolchain(auto_install=True):
    qt = find_qt()
    mingw = find_mingw(qt)
    ninja = find_ninja(qt)

    if (not qt or not mingw or not ninja) and auto_install:
        install_toolchain()
        qt = find_qt()
        mingw = find_mingw(qt)
        ninja = find_ninja(qt)

    if not qt:
        raise SystemExit("未找到 Qt 6。请安装 Qt %s (MinGW 64-bit) 并设置 QT_DIR，"
                         "或在联网环境下重跑以自动安装。" % QT_VERSION)
    if not mingw:
        raise SystemExit("未找到 MinGW-w64 (g++)。请安装到 <QtRoot>\\Tools\\mingw* 或设置 MINGW_DIR。")
    if not ninja:
        log("未找到 Ninja（仅影响构建脚本的生成器选择）。")
    return {"qt": qt, "mingw": mingw, "ninja": ninja}


def apply_env(tc):
    parts = [os.path.join(tc["mingw"], "bin"), os.path.join(tc["qt"], "bin")]
    if tc["ninja"]:
        parts.append(tc["ninja"])
    parts.append(os.environ.get("PATH", ""))
    os.environ["PATH"] = os.pathsep.join(parts)


# ---------------------------------------------------------------------------
# 构建目录（中文路径走 ASCII 联接）
# ---------------------------------------------------------------------------
def _write_marker(path):
    try:
        with open(os.path.join(ROOT, ".buildroot"), "w", encoding="ascii") as f:
            f.write(path)
    except OSError:
        pass


def build_root():
    if all(ord(c) < 128 for c in ROOT):
        marker = os.path.join(ROOT, ".buildroot")
        if os.path.isfile(marker):
            try:
                os.remove(marker)
            except OSError:
                pass
        return ROOT

    # build.ps1 records the effective build root (ASCII junction) in .buildroot
    marker = os.path.join(ROOT, ".buildroot")
    if os.path.isfile(marker):
        try:
            with open(marker, encoding="ascii") as f:
                path = f.read().strip()
        except OSError:
            path = ""
        if path and os.path.isdir(path) and \
                os.path.normcase(os.path.realpath(path)) == os.path.normcase(os.path.realpath(ROOT)):
            return path

    candidates = [JUNCTION]
    if os.environ.get("LOCALAPPDATA"):
        candidates.append(os.path.join(os.environ["LOCALAPPDATA"], "ShanCi-build"))

    for j in candidates:
        if os.path.isdir(j):
            if os.path.normcase(os.path.realpath(j)) == os.path.normcase(os.path.realpath(ROOT)):
                _write_marker(j)
                return j
            subprocess.call(["cmd", "/c", "rmdir", j])
        elif os.path.exists(j):
            subprocess.call(["cmd", "/c", "rmdir", j])

    for j in candidates:
        log("创建 ASCII 联接 %s -> %s" % (j, ROOT))
        subprocess.call(["cmd", "/c", "mklink", "/J", j, ROOT])
        if os.path.isdir(j):
            _write_marker(j)
            return j

    raise SystemExit("无法创建 ASCII 目录联接。请把工程移动到纯英文路径（如 C:\\Dev\\ShanCi）后重试。")


def build_and_deploy():
    log("构建工程 ...")
    run(["powershell", "-ExecutionPolicy", "Bypass", "-File",
         os.path.join(ROOT, "build.ps1")], cwd=ROOT)
    log("部署 Qt 运行时 ...")
    run(["powershell", "-ExecutionPolicy", "Bypass", "-File",
         os.path.join(ROOT, "deploy.ps1")], cwd=ROOT)


# ---------------------------------------------------------------------------
# 打包
# ---------------------------------------------------------------------------
def collect_files(bin_dir):
    files = []
    for base, _dirs, names in os.walk(bin_dir):
        for name in names:
            full = os.path.join(base, name)
            rel = os.path.relpath(full, bin_dir).replace("\\", "/")
            if any(rel.startswith(p) for p in EXCLUDE_PREFIXES):
                continue
            files.append((rel, full))
    files.sort(key=lambda t: t[0])
    return files


def build_payload(bin_dir, out_path):
    files = collect_files(bin_dir)
    if not files:
        raise SystemExit("build\\bin 为空，请先运行 build.ps1 与 deploy.ps1")

    h = hashlib.sha1()
    for rel, full in files:
        st = os.stat(full)
        h.update(("%s|%d|%d;" % (rel, st.st_size, int(st.st_mtime))).encode("utf-8"))
    tag = h.hexdigest()[:16].encode("ascii")

    total_raw = 0
    total_comp = 0
    with open(out_path, "wb") as out:
        out.write(MAGIC)
        out.write(struct.pack("<I", FMT_VERSION))
        out.write(struct.pack("<H", len(tag)))
        out.write(tag)
        out.write(struct.pack("<I", len(files)))

        for idx, (rel, full) in enumerate(files):
            with open(full, "rb") as f:
                data = f.read()
            comp = zlib.compress(data, COMPRESS_LEVEL)
            pathb = rel.encode("utf-8")
            out.write(struct.pack("<H", len(pathb)))
            out.write(pathb)
            out.write(struct.pack("<QQI", len(comp), len(data),
                                  zlib.crc32(data) & 0xFFFFFFFF))
            out.write(comp)
            total_raw += len(data)
            total_comp += len(comp)
            if idx % 200 == 0:
                print("  打包 %d/%d" % (idx, len(files)), end="\r", flush=True)

    print("")
    log("打包完成: %d 个文件, 原始 %.1f MB -> 压缩 %.1f MB"
        % (len(files), total_raw / 1048576.0, total_comp / 1048576.0))
    return tag.decode("ascii")


def build_launcher(bin_root, dist_dir, payload_path, output_path, tc):
    mingw_bin = os.path.join(tc["mingw"], "bin")
    gcc = os.path.join(mingw_bin, "gcc.exe")
    windres = os.path.join(mingw_bin, "windres.exe")
    boot_src = os.path.join(ROOT, "tools", "bootstrapper", "Bootstrapper.c")

    rc_path = os.path.join(dist_dir, "payload.rc")
    res_obj = os.path.join(dist_dir, "payload_res.o")
    with open(rc_path, "w", encoding="ascii") as f:
        f.write('#include <windows.h>\n')
        f.write('%d RCDATA "%s"\n' % (101, os.path.basename(payload_path)))

    log("编译资源 ...")
    run([windres, "--include-dir", dist_dir, "-O", "coff",
         "-o", res_obj, rc_path], cwd=dist_dir)

    log("编译启动器 ...")
    run([gcc, "-O2", "-s", "-static", "-mwindows",
         "-o", output_path, boot_src, res_obj,
         "-lz", "-lshell32"], cwd=bin_root)


def main():
    no_build = "--no-build" in sys.argv
    no_auto = "--no-auto-install" in sys.argv

    tc = resolve_toolchain(auto_install=not no_auto)
    log("Qt     : " + tc["qt"])
    log("MinGW  : " + tc["mingw"])
    if tc["ninja"]:
        log("Ninja  : " + tc["ninja"])
    apply_env(tc)

    if not no_build:
        build_and_deploy()

    root = build_root()
    bin_dir = os.path.join(root, "build", "bin")
    if not os.path.isdir(bin_dir):
        raise SystemExit("未找到构建输出: " + bin_dir)
    if not os.path.isfile(os.path.join(bin_dir, "ShanCi.exe")):
        raise SystemExit("未找到 ShanCi.exe，请先构建并部署")

    dist_dir = os.path.join(root, "dist")
    os.makedirs(dist_dir, exist_ok=True)

    payload = os.path.join(dist_dir, "payload.bin")
    tag = build_payload(bin_dir, payload)

    output = os.path.join(dist_dir, OUTPUT_NAME)
    build_launcher(root, dist_dir, payload, output, tc)

    for tmp in ("payload.bin", "payload.rc", "payload_res.o"):
        try:
            os.remove(os.path.join(dist_dir, tmp))
        except OSError:
            pass
    with open(os.path.join(dist_dir, "payload-tag.txt"), "w", encoding="ascii") as f:
        f.write(tag + "\n")

    size_mb = os.path.getsize(output) / 1048576.0
    log("成功！单文件可执行程序：")
    log("  " + output + "  (%.1f MB)" % size_mb)
    log("双击即可启动系统（首次运行会解压到 %%LOCALAPPDATA%%\\ShanCi\\app）。")


if __name__ == "__main__":
    main()
