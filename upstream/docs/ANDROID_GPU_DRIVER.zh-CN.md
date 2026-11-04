# Android Vulkan 驱动、诊断与复现

2026-10-15。可选用户 T30、完整动态 Vulkan 分发及明确子组64选择已构建安装。新默认系统 `test.py`、同App UID system默认和strict T30 required64属性/完整GPU suite/profile均真实exit0。T30的真实Maxwell128线程及32/48部分活跃工作组、256个片元API像素均已通过。同APK/同位置营地已证实T30脚下矩形消失、切System再现，GPU均16.05ms对24.46ms；CPU索引版T30均21.805FPS，仍未60。以下旧阶段证据保留，最新状态见文末。

root 已实际完成 O3 Linux LVP 全部回归，日志 `~/dm-work/tools/android-tests-known-copy-host.log`：全部原 gates，以及新增完整 known-copy/部分覆盖/真实跨批/事件/计数器/真实 GPU 后写回退均 PASS。此 Host 结果独立于新 Android dispatch，不能代替 Android 验证。

## 代码和构建

`dm/android/native/gpu_driver.c/.h` 持有 loader handle 和 GIPA/GDPA；`vulkan_dispatch.h` 覆盖所有现用 Vk 调用，包含测试用 FillBuffer/FenceStatus 和属性查询 FormatProperties2，共110项。Android 两个前端都使用自己的完整分发表，移除直接 `-lvulkan`；Linux Host 原链接不变。headless 未启用的 WSI 入口可为空，真实 surface 路径必须完整。timeline gate 仍只启用实际 KHR extension，并经所选 device 的 GDPA 获取 KHR 入口；所有原 GPU gates 保留。

`tools/build_gpu_driver.py` 从固定 libadrenotools8ba23b42d7、liblinkernsbypassaa3975893d 源码构建 `libadrenotools.so` 及四个 hooks。源码默认从指定研究缓存或 `~/dm-work/tools/android/sources` 获取，缺失时下载官方固定commit归档；显式源码覆盖通过 DM_ADRENOTOOLS_SOURCE/DM_LINKERNSBYPASS_SOURCE，build.json记录实际目录和 requested_commit，不把覆盖目录冒充已验证commit。全部 C++ runtime 静态链接，16KiB LOAD 对齐；版权许可进入 APK assets/licenses。

`tools/build_gpu_fixtures.py` 从**当前源码**分别编译/链接两个 PIE ELF：

- `libdmgpudriverprobe.so`：真实 loader/driverID/subgroup/format 属性，以及 Instance→Device 分发。
- `libdmgputestprobe.so`：现有完整 vulkan_test.c、所有包含的回归用例和真实 GPU gates，链接 gpu_driver.c，完全不直接链接系统 Vulkan。

它们只是从 APK nativeLibraryDir 执行的诊断组件，没有 Activity，不是游戏 loader。逐 object 的 .d/.cmd 跟踪源码及所有递归 inc/header，链接记录于 `dm/android/build/gpu-fixtures/build.json`，不会复制研究目录里的旧 ELF。构建依赖加文件锁；无变更时不反复改写共享库。GLSL fixture cache 单独进入 assets/gpu-test-shaders，不覆盖游戏 Maxwell fullshader cache。

在仓库根目录：

```sh
python3 dm/android/tools/build_gpu_driver.py
python3 dm/android/tools/build_gpu_fixtures.py
# 正常build.py --game-run --game-source <现有合法改写产物目录> 已自动包含以上依赖；不要加--install。
```

最终 APK 仍执行16KiB zipalign和apksigner验证。两个诊断 ELF 有真实 main/PT_INTERP、PIE 标志；`.so`后缀仅供 APK 原生库提取。Android10/API29+禁止 App 直接 execve 可写 home 内文件，因此测试从自身 APK 提取的只读 nativeLibraryDir 执行。[Android 官方行为说明](https://developer.android.com/about/versions/10/behavior-changes-10#execute-permission)

本机完整构建使用已安装 lz4/capstone 的 Android venv；系统 Python3.14 缺 lz4 的首轮失败日志保留，没有为此更改源码。最终实际执行且 exit0：

```sh
~/dm-work/tools/android-venv/bin/python dm/android/tools/build.py \
  --game-run --game-source ~/dm-work/build/android/game \
  --shader-cache ~/dm-work/build/android/shaders-subgroup64/spirv
```

产物 `dm/android/build/D2R-Android-port.apk` 为94,345,751字节；固定副本 `~/dm-work/research/turnip-loader-reference/final-unified-driver-diagnostics-built.apk`。`final-apk-build.log` 是完整构建输出，`final-apk-report.json`、`final-apk-entries.json` 和 `final-apk-native/*.readelf.txt` 记录 APK 实际提取产物：12个原生 ELF，含五个自编 BSD 驱动库及两个诊断 PIE；全部 LOAD 至少16KiB对齐；主库和两诊断无 DT_NEEDED libvulkan、无未解析 vk入口；两诊断有 PT_INTERP、非零入口及 PIE。51个专属 GLSL cache文件和两份依赖许可已打包。zipalign `-c -P 16 4` 与 apksigner `verify --verbose` 均 exit0。完整 fixture 的递归 `.d` 已实际包含本轮 known_copy_snapshot_test.inc、async/probe 和 dispatch 头；对应精确 compiler/link argv 在 `dm/android/build/gpu-fixtures/build.json`。

上述产物审计只验证离线产物和符号归属；其后root已完成实际安装和系统/T30诊断，结果见下方最新证据。

## 选择、身份和回退

默认不配置即系统 Vulkan。正式游戏 boot 已在 GPU 初始化前设置 DM_ANDROID_LIBDIR 和 DM_TEMP，无需改输入层。可选配置为 `DM_TEMP/gpu-driver.conf`，本应用是自身 `files/temp/gpu-driver.conf`：

```ini
directory=/data/user/0/org.d2r.android.port/files/temp/driver/
library=vulkan.purple.so
require_custom=1
subgroup_size=64
```

环境 DM_VK_DRIVER_DIR、DM_VK_DRIVER_NAME、DM_VK_DRIVER_REQUIRE 对应覆盖；DM_VK_DRIVER_CONFIG 可指定配置文件。REQUIRE=1先于配置解析生效；配置即使前行错误，也继续读取后置 require_custom=1，再报告首错。DIR/NAME覆盖在成功解析文件后才执行，因此独立system测试应将CONFIG显式设为/dev/null以避开任何现存配置语法错误，再设DIR为空、REQUIRE=0；不会删除或重命名配置。严格测试不能悄悄回系统。无严格策略时 custom 初始化失败先用旧分发表销毁尚未提交的 device/surface/instance，再完整打开系统 handle；每次回退清除旧 PFN。驱动仅能在新进程初始化时选择，不能热切换。

T30 必须在**本应用内部**可执行文件目录，hooks 必须位于自身实际 nativeLibraryDir。不会引用 Eden 私有数据、替换系统驱动、改别的游戏配置或密钥。当前此路径针对用户已提供的 T30（minApi30、vulkan.purple.so），不是全手机驱动更新。

adrenotools 内部可能加载失败后返回非NULL系统loader，因此必须再查真实 VkPhysicalDeviceDriverProperties：要求 VK_DRIVER_ID_MESA_TURNIP，日志打印 actual driverID/name/info/API/subgroup。deviceName含Adreno或handle非NULL都不能算Turnip成功。[adrenotools API](https://github.com/eden-emulator/libadrenotools/blob/8ba23b42d7/include/adrenotools/driver.h)，[内部回退](https://github.com/eden-emulator/libadrenotools/blob/8ba23b42d7/src/hook/hook_impl.cpp)

真实游戏仍要求 shader cache 的 `host-subgroup-size.txt` 是 16、32、64 或 128，并与实际 shader 执行 subgroup 相同。PanVK 在 Xiaomi 11T 上报告物理 subgroup16；为该驱动准备数据包时应把 profile 设置为16（`build_shaders.py --host-subgroup-size 16` / package `--subgroup 16`），不可把64 profile直接冒充16。未指定驱动 subgroup 时使用真实物理缺省；指定64等档位时必须实际支持、启用扩展并为每个VS/FS/CS管线声明该值。独立 GLSL fixture 没有离线Maxwell cache，仍做真实driverID检查和GPU断言；这不是绕过游戏profile校验。

## 连接恢复后的 root 验证顺序

本节保留验证顺序和可复现脚本，root统一操作设备；最新实际结果见下方。每次安装后重新获取实际nativeLibraryDir。

1. 先让 `~/dm-work/tools/android-venv/bin/python dm/android/tools/test.py` 在新默认系统分发下完成全部原 Android 回归；系统属性probe也已加入。保存完整输出，不以单个复制/属性PASS替代完整suite。
2. 审核后安装带 hooks + 两个 PIE 诊断组件的新 APK，保留现有应用数据。先不写T30配置，验证默认系统属性与完整GPU fixture。
3. 获取本次安装**实际**native库路径，再以run-as向自身files/temp/driver流式复制用户T30，使用本次诊断env显式strict选择，不写正式game配置。以同App UID跑strict T30属性，然后全部GPU fixture；失败必须保存实际driverID/日志并停止宣称Turnip通过。
4. 独立属性组件传入真实设备game-cache目录，核对实际subgroup/profile。所有上述检查通过后才为本轮game写入strict配置并开新进程。这些是headless检查，不覆盖Android WSI、swapchain/present或游戏；运行游戏时再次确认实际driver身份/profile，采集同营地像素/帧时间和R16探针。比较性能时先对齐工作档和尺寸。

设置 D2R_TEST_SERIAL 为 root 恢复后实际选择的目标，D2R_TEST_DRIVER先设system；之后单独设turnip，并让D2R_USER_TURNIP_SO指向从用户已提供zip解出的vulkan.purple.so，再分别执行准备/验证。不要读取或复制密钥。下面准备脚本在已安装新APK后运行；先打印确认自身nativeLibraryDir，候选必须实际含主库、adrenotools、全部四hooks及两个诊断ELF，再以run-as确认两个ELF执行权限：

```python
import json, os, pathlib, re, shlex, subprocess, tarfile, io
adb = str(pathlib.Path.home() / 'dm-work/tools/android/platform-tools/adb')
base = [adb, '-s', os.environ['D2R_TEST_SERIAL']]
pkg = 'org.d2r.android.port'
mode = os.environ.get('D2R_TEST_DRIVER', 'system')
if mode not in ('system', 'turnip'): raise ValueError('D2R_TEST_DRIVER must be system or turnip')
q = shlex.quote
def shell(command):
    return subprocess.check_output(base + ['shell', command], text=True).strip()
def own(command, data=None):
    subprocess.run(base + ['shell', '-T', 'run-as ' + q(pkg) + ' sh -c ' + q(command)],
                   input=data, check=True)
dump = shell('dumpsys package ' + q(pkg))
exact = re.findall(r'^\s*nativeLibraryDir=(\S+)', dump, re.M)
legacy = re.findall(r'^\s*legacyNativeLibraryDir=(\S+)', dump, re.M)
candidates = exact + [p + '/arm64' for p in legacy]
native = None
for candidate in candidates:
    files = [candidate + '/' + name for name in (
        'libdmandroid.so', 'libadrenotools.so', 'libhook_impl.so', 'libmain_hook.so',
        'libfile_redirect_hook.so', 'libgsl_alloc_hook.so',
        'libdmgpudriverprobe.so', 'libdmgputestprobe.so')]
    result = subprocess.run(base + ['shell', 'ls -l ' + ' '.join(map(q, files))],
                            capture_output=True, text=True)
    if result.returncode == 0:
        native = candidate
        print(result.stdout)
        break
if native is None:
    raise RuntimeError('Actual extracted native library directory not found; stop')
own(' && '.join('test -x ' + q(native + '/' + name) for name in (
    'libdmgpudriverprobe.so', 'libdmgputestprobe.so')))
data_root = shell('run-as ' + q(pkg) + ' pwd')
if not data_root.startswith('/data/'):
    raise RuntimeError('Unexpected run-as data path; stop')
print('nativeLibraryDir:', native, 'dataRoot:', data_root)
own('mkdir -p files/temp/driver files/temp/gpu-test-shaders')
if mode == 'turnip':
    own('umask 077; cat > files/temp/driver/vulkan.purple.so',
        pathlib.Path(os.environ['D2R_USER_TURNIP_SO']).read_bytes())
cache = pathlib.Path('dm/android/build/gpu-test-shaders')
archive = io.BytesIO()
with tarfile.open(fileobj=archive, mode='w') as tar:
    for path in sorted(cache.iterdir()):
        if path.is_file(): tar.add(path, arcname=path.name)
own('tar -xf - -C files/temp/gpu-test-shaders', archive.getvalue())
record = {'serial': os.environ['D2R_TEST_SERIAL'], 'native': native, 'data_root': data_root, 'mode': mode}
pathlib.Path('dm/android/build/gpu-fixtures/device-env.json').write_text(json.dumps(record, indent=2))
```

此脚本的system轮只准备自身temp/GLSL缓存；turnip轮才复制用户driver，两轮均不写gpu-driver.conf。driver是供dlopen映射的共享库，umask077得到的读写权限不妨碍加载，不能因为没有exec位而判定失败。没有启动、退出或改任何存档。每次重新安装APK后须重新获取native路径。属性/full fixture执行脚本如下，两轮CONFIG均显式/dev/null，不读取现存私有配置；system轮选系统并关闭strict，turnip轮指定内部driver目录与NAME、REQUIRE=1。分别保存独立完整stdout/stderr，从APK目录exec，driver/cache从自己的内部files读取：

```python
import json, os, pathlib, shlex, subprocess
record = json.loads(pathlib.Path('dm/android/build/gpu-fixtures/device-env.json').read_text())
base = [str(pathlib.Path.home() / 'dm-work/tools/android/platform-tools/adb'), '-s', record['serial']]
q = shlex.quote
mode = record['mode']
if mode not in ('system', 'turnip'): raise ValueError('Unexpected recorded driver mode')
prefix = ('run-as org.d2r.android.port env '
          'DM_ANDROID_LIBDIR=' + q(record['native']) + ' '
          'DM_TEMP=' + q(record['data_root'] + '/files/temp') + ' '
          'DM_VK_DRIVER_CONFIG=/dev/null '
          'DM_VK_DRIVER_NAME=vulkan.purple.so '
          'DM_VK_DRIVER_REQUIRE=' + ('1' if mode == 'turnip' else '0') + ' '
          'DM_VK_DRIVER_DIR=' + q(record['data_root'] + '/files/temp/driver' if mode == 'turnip' else '') + ' ')
for name in ('libdmgpudriverprobe.so', 'libdmgputestprobe.so'):
    command = prefix
    if name == 'libdmgputestprobe.so':
        command += 'DM_VK_SHADERS=' + q(record['data_root'] + '/files/temp/gpu-test-shaders') + ' '
    binary = record['native'] + '/' + name
    command += '/system/bin/sh -c ' + q('exec ' + q(binary))
    result = subprocess.run(base + ['shell', command], capture_output=True, text=True)
    pathlib.Path('dm/android/build/gpu-fixtures/' + mode + '-' + name + '.device.log').write_text(result.stdout + result.stderr)
    print(name, 'exit', result.returncode)
    if result.returncode: raise SystemExit(result.returncode)
    if name == 'libdmgpudriverprobe.so' and 'GPU driver properties and dispatch PASS' not in result.stdout:
        raise RuntimeError('Properties binary did not produce its real completion marker')
    if name == 'libdmgputestprobe.so' and 'exact red 255 instead of stale 248 PASS' not in result.stdout + result.stderr:
        raise RuntimeError('Full GPU suite did not produce its final regression result')
# 可选：用属性组件单独校验实际游戏缓存profile，不替换完整fixture的GLSL目录。
# Optional: check the actual game-cache subgroup profile using the properties component.
game_cache = os.environ.get('D2R_TEST_GAME_SHADER_DIR')
if game_cache:
    command = prefix + '/system/bin/sh -c ' + q('exec ' + q(record['native'] + '/libdmgpudriverprobe.so') + ' ' + q(game_cache))
    result = subprocess.run(base + ['shell', command], capture_output=True, text=True)
    pathlib.Path('dm/android/build/gpu-fixtures/' + mode + '-game-profile.device.log').write_text(result.stdout + result.stderr)
    print('game profile', 'exit', result.returncode)
    if result.returncode: raise SystemExit(result.returncode)
    if 'GPU driver properties and dispatch PASS' not in result.stdout:
        raise RuntimeError('Game profile binary did not produce its real completion marker')
# root明确准备下一步game时才设此开关；此前任何失败均已退出，且尚未持久化配置。
# Activate only for the next game run; earlier failures exit before writing configuration.
if os.environ.get('D2R_ACTIVATE_CUSTOM_DRIVER') == '1':
    if mode != 'turnip' or not game_cache:
        raise RuntimeError('Activation requires strict Turnip fixtures and actual game-cache profile PASS')
    def own(command, data=None):
        subprocess.run(base + ['shell', '-T', 'run-as org.d2r.android.port sh -c ' + q(command)],
                       input=data, check=True)
    own('if [ -f files/temp/gpu-driver.conf ] && [ ! -e files/temp/gpu-driver.conf.before-turnip ]; '
        'then cp -p files/temp/gpu-driver.conf files/temp/gpu-driver.conf.before-turnip; fi')
    config = ('directory=' + record['data_root'] + '/files/temp/driver/\n'
              'library=vulkan.purple.so\nrequire_custom=1\n')
    own('umask 077; cat > files/temp/gpu-driver.conf', config.encode())
    print('Strict Turnip game configuration saved; start the game in a new process')
```

default系统诊断通过CONFIG=/dev/null、DIR=''、REQUIRE=0明确选system；即使此前准备了本轮T30配置，也不需改该文件。属性组件发现requested custom已fallback即报FAIL；完整suite的strict配置也禁止fallback假通过。T30属性与完整GLSL fixture通过只证明该测试路径，尚未校验离线Maxwell游戏cache；可将D2R_TEST_GAME_SHADER_DIR设为设备上真实游戏cache目录（本机通常`/sdcard/Android/data/org.d2r.android.port/files/shaders`），在上述可选独立属性轮核对host-subgroup-size.txt。真实游戏初始化会再次强制此检查；大小不符需重建对应完整cache，不能改profile数字冒充翻译。App UID权限、T30属性和全部像素/时序断言仍需要实测，不由这里的命令文本保证成功。

这两个诊断进程的env仅作用于本次run-as命令，不会传给另一次`am start`的游戏进程。只有root为下一步game明确设置D2R_ACTIVATE_CUSTOM_DRIVER=1，并且strict Turnip两项fixture及D2R_TEST_GAME_SHADER_DIR profile实际通过，上述脚本才备份并写入自身gpu-driver.conf；任一测试失败都在写配置前退出。严格game轮应在新进程中启动并检查实际driver日志。之后若比较system游戏，先在游戏已退出时将**本轮创建的自身配置**保留为.disabled/恢复首次备份，再开新进程；不要覆盖别人的配置，或将游戏fullshader目录换成GLSL缓存。

## 分辨率与现有证据

Eden当前配置是Handheld/Normal、Res1X（源码仅1/1缩放），VI向游戏报1280×720；实际guest输出/动态3D RT现场仍未知。SurfaceFlinger只证实最后Android呈现3392×2400，不能当作3D尺寸。

port默认DM_OPMODE=1 Console/Boost，front_game_res(1)=1600×900，最新营地probe实测主/深度RT也是1600×900；已有DM_OPMODE=0对应1280×720。900p与720p名义像素比1.5625，不能把Eden30与port19之差全部归给驱动。显示能力/帧间隔判断修复已随本候选离线构建，实机效果仍待测；这与driver选择独立。后续公平对照先用现有DM_OPMODE=0并记录实际RT，再分别换driver和比较相同场景。

研究证据保留于 `<LOCAL_WORK>/research/turnip-loader-reference/`：官方依赖、用户T30副本、离线build日志/精确argv/ELF检查；`eden-android-reference/`保留用户实际营地30FPS三张截图及当前T30元数据。较早离线main/属性/fullfixture均无DT_NEEDED libvulkan/undefined vk符号，所有LOAD align0x4000；这仅证明构建与符号归属，**不证明新Android运行正确或性能达标**。


## 最新真机结果与启动脚本修正

新默认完整回归实际exit0，日志 `~/dm-work/tools/android-tests-unified-dispatch-system.log`。94MB统一候选安装后，以本应用UID从真实APK只读native目录执行两个组件，system/strict T30全部实际GPU像素、门控、查询、材质和known-copy断言通过：`dm/android/build/gpu-fixtures/{system,turnip}-libdmgputestprobe.so.device.log`。system实际driver8/Adreno830/API1.3.284/subgroup64；T30实际driver18/PurpleVK26.3.0/git62ac221a33/API1.4.359/subgroup128。两驱动R16_UNORM compare0、D16 compare1。

首轮脚本把含==的APK随机安装路径直接接在toybox env赋值后，env将它误当成另一项赋值，返回0只打印环境；这不算执行或PASS。原输出保存为launch-not-executed前缀，profile轮exit126。现脚本让env执行固定/system/bin/sh，再由exec启动安全引号的二进制，并同时检查实际属性/最终GPU断言标记；重新执行取得以上结果。

同App UID的run-as进程不能直接读取本设备external游戏缓存profile，T30附加profile轮明确Permission denied；system验证函数只验证身份，不强制profile读取，因此system附加轮exit0也不能证明它读到了profile。shell实际cat当前游戏host-subgroup-size.txt为64，并已保存准确内部诊断副本，后续T30必须对真正新翻译的128缓存核对，不能修改数字冒充翻译。真实Activity自身能读取其external缓存，初始化仍强制custom profile匹配。

当前 `shaders-subgroup128` 独立full缓存6931/6931 Vulkan1.1val和反射完成，专项独立审核 .meta全字节一致、.res.json语义一致，只有4个CS随线程组变化，旧full64保持。新真正Maxwell32/64/128对照probe已离线编译，将从APK自身目录用strict T30实测；尚不宣称128 GPU适配通过或游戏FPS达标。

系统full64游戏现已真正识别144Hz屏幕、原NVN使用display模式/固定60Hz，主机档营地实际1600×900、坐标4009,4929，截图20.5FPS/48.8ms，稳态约19–21FPS；R16cube六面全0、地面矩形斑块仍在。证据 `~/dm-work/research/android-camp-unified-system-900p`。新launch-settings选择handheld0后，实际中间UI/主深度/HDR目标为1280×720，最终交换链底图仍1600×900并裁剪1280×720；须用实际中间目标判断工作分辨率，不能只看WindowInitialize。最初handheld采集仍在主菜单，已如实改名android-menu-unified-handheld-system；随后真实720p营地和typed-clear营地均19–21FPS，地面斑块仍在。

## 明确64选择的新生产真机回归

检查点 `~/dm-work/build/android/checkpoints/subgroup-policy-probe-system720p-before-cpu-index.apk`，94,837,710字节，正常build.py生成、安装Success。同名生产ELF和source.tar.gz也已冻结，便于隔离后续CPU优化。完整新默认系统回归 `~/dm-work/tools/android-tests-subgroup-policy-system.log` exit0；当前源码生成的APK诊断组件再次以App UID执行，system默认、strict T30 required64完整GPU套件都exit0，全部原timeline/材质/像素/query/known-copy/typed-clear断言保留。

生产T30属性真实driver18、物理default128、explicit_request64、shader_selected64、device_policy_enabled1。API1.1路径真实枚举并启用EXT，保留原核心功能、swapchain/timeline链；computeFullSubgroups0、所有实际VS/FS/CS使用required64且flags不设FULL/VARYING。实际full64游戏profile内部准确副本匹配，真实Activity启动时仍重新读取自己的external缓存。6931份Android能力安全版full64缓存全部重新校验，只有未启用且未使用的Float64/Interlock声明被清除，其余可执行指令和精度逐字不变。

默认128的真实Maxwell新译法曾在T30出现高64lane投票错误，不能靠改物理属性解决。明确64的128线程×22列真实运算为0错误；追加LocalSize32/48部分活跃工作组也实际通过，computeFullSubgroups0/flags0，Local48仅一项规范未定义的inactive-source SHFL数值不设真值，全部有效标志/ballot/ALL/ANY/EQ仍严格验证。独立片元API探针256个真实RGBA32_UINT像素均size64、0错误；它只证明实际API选择，不冒充Maxwell片元语义。

原始生产日志 `dm/android/build/gpu-fixtures/turnip-required64-{libdmgpudriverprobe.so,libdmgputestprobe.so,game-profile}.device.log`；独立部分组日志/原始CSV/exit位于 `~/dm-work/research/subgroup64-partial-probe/device-turnip-partial.*`。实际运行脚本 `turnip-loader-reference/test-appuid.py` 现支持 `D2R_TEST_SUBGROUP_SIZE=64`，仍以CONFIG=/dev/null隔离现存配置，日志增加required64标签；正文旧脚本保留此前复现步骤。明确请求时，任何custom加载/验证失败都响亮拒绝，root修复尾部唯一漏检分支，独立撤回该行的负例确实检测到非法系统回退。

所有上述检查通过后root才写入自己的strict T30配置及subgroup_size64，开始同档位720p营地实测；尚未根据headless PASS宣称游戏渲染正确或达到60FPS。

## 同APK营地与同原始输入CS交叉对照

冻结before-index APK，掌机真实720p、同角色4009,4929：`android-camp-turnip-required64-720p` 正常900–1800帧剔除probe1500，FPS均21.44、GPU16.0487ms，脚下矩形消失；同APK改System，`android-camp-new-policy-system-default-720p` FPS均20.026、GPU24.4633ms、矩形再现。两侧108/109地面材质逐字节相同，cube44六面均0且当前未发现44→45物理alias。CPU索引版另采 `android-camp-index-cs-turnip-720p` 同位置均21.80467FPS/GPU16.08067ms，不能混成一次APK控制，也不能宣称全部渲染或60FPS完成。

新显式CS探针在真实dispatch之前/之后立即复制实际bound buffer/view，显式frame+request第三行key才开启，正常帧不捕获。T30 frame1500原b54分类CS共一次4×5×1dispatch，8 pre/2 post。Host LVP使用原shader+这些真实输入，与游戏输出counts5120和indices1310720全部逐字节相同；640格独立分类、子集、重复检查均通过。

相同T30 pre在设备只读APK诊断进程两路重放：strict T30 required64与Host/游戏全部逐字节相同；System driver8执行完成，但counts44字节、indices53字节不同，独立oracle exit1（31格子集和31格分类错误）。System即时输入的另一半也已闭环：System pre→System逐字复现自身坏游戏post；System pre→T30与Host逐字一致且规则全0，相对坏游戏post为counts29字节/indices35字节不同。两份独立输入均稳定复现System错类，四路broad半部都逐字一致；完整汇总 `cs-light-bin-replay/fourway-summary.json`。原始输出、命令、driver身份、实际退出码与独立审计均在该研究目录。不能把dispatch执行exit0称数值通过；此结论只限原b54/冻结输入/该设备驱动。

原分类CS无ballot/shuffle/原子/屏障，不能用另一条默认子组Builtin异常解释此计算差异。尚未确认错误指令或驱动编译机制；最小测试只用于独立诊断，不替换原游戏shader、不改精度/数据、不按样本硬编码修复。

最小bitcast保存样例已在两个驱动真正执行并逐字节检查全部通过，不能据此声称原CS是denormal清零。当前用户设备已恢复strict T3064，正常新提交统计APK营地约22FPS、GPU16ms，probe0关闭；CPU22ms中kernel约16.8ms，优先继续性能分账。新增统计实测与最新安装检查点详见移植进度文末。

后续最窄locals、标准single-block、fullSSA三份独立合法候选已实际System/T30执行：T30三份全buffer与Host一致；System前两份仍44/53B差异，fullSSA10/36B差异且仍31格分类/子集违规，未构成修复。原cache和生产译器保持，研究输出在 `cs-light-bin-replay/reduction/`。

最新reader/dirty正常APK营地真实稳态21.493FPS/GPU15.943ms，frame900混加载明确剔除。实际1308次/24.66MiB读取约14.2ms，MP约1004次/all-thread wall7ms，分别口径不能相加为内核总量；优先优化CPU读取，继续与原图形同步/内容失效保持一致。当前尺寸桶APK probe0关闭，strict T3064；独立映射基准仍是研究组件，不进入game流程。未达60FPS。

## 内置 Turnip 与启动界面切换（2026-10-18）

**实测结论：人物周边"硬边方块状光照"是驱动侧差异。** 同一存档、同一位置、同一份源码：系统自带的 Qualcomm Adreno 驱动（`driver_id=8`）会出现成块的亮度不一致（人物附近最明显，逐帧跳变，看着像在闪）；换成 Turnip 后画面平滑正常，帧率同为 60 上下（`[vulkan-frame-stability]` 帧间隔 p50 16.65 ms、无超 33 ms 帧）。

**光照的实际载体**（由真机探针抓出，非推测）：游戏把光照算进一张 **32×8×32 的 RGBA16F 三维贴图**（计算着色器以存储图像写入，片元着色器每帧采样约 1882 次，采样器是**线性过滤**）。可见范围内正好约 32 个格子 ≈ 实测的 106 屏幕像素方块 —— 方块尺寸与这张贴图的格子完全对上。同一份着色器与数据在 Turnip 上正常，所以问题在驱动对这张三维存储图像的处理，不在移植的读数、上传或过滤。

**Turnip 随包分发**：驱动放在 `dm/android/prebuilt/turnip/`（`vulkan.purple.so` + 上游 `meta.json`），默认由 `build.py --turnip` 打进 APK；`--no-turnip` 会移除它，启动器自动隐藏该选项。Turnip 仅面向带 KGSL 的 Qualcomm 设备。

**Turnip 启动界面切换**：选择保存在启动器自己的 `launcher-state.conf`（`launch-settings.conf` 由 native 严格白名单解析，不能塞新键）。KGSL 设备选择 Turnip 后，启动器把 `assets/turnip/vulkan.purple.so` 复制到应用内部 `files/temp/driver-turnip/`，写入 `files/temp/gpu-driver.conf`（`backend=turnip` / `library=vulkan.purple.so` / `require_custom=1` / `subgroup_size=<数据包档位值>`）；选择系统驱动则删除该配置。**子组尺寸必须匹配数据包的 `shaders/host-subgroup-size.txt`**，native 会对照实际物理能力校验；不匹配就拒绝启动，不能静默画错。选择自定义驱动时显式 strict，加载或 subgroup/profile 检查失败不会偷偷切换；`system` 是玩家可选择的回退。

真机验证（2026-10-18）：界面切换 → 点启动 → `[vulkan-loader] requested=Turnip selected=Turnip identity verified`（`PurpleVK 26.3.0-devel`、`shader host subgroup=64 matches`）；切回系统驱动 → 配置被删除 → `driver configuration not found` → `requested=system selected=system`。

## 三种 Vulkan 后端（System / PanVK / Turnip）

启动器提供三种模式：`system` 是始终可选的系统 Vulkan；`panvk` 是 JimVulkan PanVK r9，仅在 Android API 31+ 且存在 `/dev/mali0` 的 Mali kbase 设备显示；`turnip` 是内置 Turnip T30，仅在存在 `/dev/kgsl-3d0` 且满足 API30 时显示。因此 Xiaomi 11T（Mali-G77 MC9）可选择系统驱动或实验性 PanVK；Turnip 仍随 APK 保留，但不会伪装成 Mali 可用选项。

PanVK 包来自 [JimVulkan mali-panvk v1.5.0 / r9](https://github.com/JimVulkan/mali-panvk/releases/tag/v1.5.0)，构建固定校验 ZIP 与 `.so` SHA-256，并随 APK 分发上游 `NOTICE.txt`。r9 声明 Android API 31+，上游列出 Mali-G77 支持。native 端通过 AdrenoTools 注入 Android 平台 Vulkan loader，并严格要求最终 `driverID` 为 `VK_DRIVER_ID_MESA_PANVK`；PanVK namespace 会链接 Android 平台依赖，不把私有系统 `.so` 复制进 APK。启动器与 native loader 均严格 `require_custom=1`，加载失败不暗中切换；`system` 仍是玩家主动选择的回退。

历史记录：以下 r8 / FristOneRR 设备验证只代表旧版直接 ICD 加载路径，不代表当前 r9 的 Android 平台 loader 路径。

2026-11-03 构建后的 `run-as` App UID fixture（不代表 Java Activity 的 classloader namespace）：系统驱动（ID 9）与 PanVK（ID 20）均通过 instance、physical-device、device、queue 与 `vkDeviceWaitIdle` 检查；PanVK 实际报告 Mali-G77 MC9、API 1.3、subgroup 16。PanVK 的 subgroup16 游戏 profile 检查通过；已导入数据包中的 profile 也是16；profile64 按预期因与物理 subgroup16 不匹配而拒绝。PanVK 完整 Vulkan 回归组件尚未通过：在 `tests/vulkan_test.c:52` 的 1 秒 `mtl_wait` 断言中 abort。系统驱动回归组件也未全通过，在之后的 `uniform_arena_reference_test` 断言中 abort。因此只确认两个 loader/device-init 路径；**没有验证 Android surface/swapchain present 或真实 D2R 进入实战**。上游把 PanVK 标为 non-conformant，仍属实验选项，不承诺兼容性或性能。构建可用 `--no-panvk` / `--no-turnip` 显式排除对应包；默认 APK 打包二者。
### PanVK Android linker namespace fix and Xiaomi 11T smoke (2026-11-03)

The installed app log exposed the actual launch failure: `PanVK dlopen: library "libhardware.so" not found ... in namespace clns-4`. The file exists on the device but is not in the Android app namespace's public-library list. This was not a PanVK checksum or Mali-G77 detection failure. PanVK also has a Termux-specific `RUNPATH`; bundling `libhardware.so` would be an unsafe, non-portable workaround.

The APK now includes `libpanvk_namespace.so`, built with the pinned `liblinkernsbypass` source. It creates a shared linker namespace rooted at the app-private PanVK directory, links that namespace to Android's default system libraries, then loads the PanVK ICD by basename. It does not copy private Android libraries into the APK. Strict PanVK selection and the explicit System fallback remain unchanged.

Validation on the Xiaomi 11T: APK `0.6.67` installed over the existing app with its matching signing key, preserving app data. The strict PanVK `gpu-driver-properties` smoke, run as the app UID, reported ICD interface 7, `Mali-G77 MC9`, driver ID 20, Vulkan 1.3.366, subgroup 16, and completed instance/device/queue initialization plus `vkDeviceWaitIdle` (`GPU driver properties and dispatch PASS`).
The same app-UID probe with no custom-driver config selected the system driver (ID 9) and also passed device creation and idle.

This smoke validates ICD loading and device initialization, not Android surface/swapchain/presentation or D2R gameplay. The phone was still at the keyguard during this check; the saved launcher selection remains `system`. Do not treat PanVK gameplay compatibility as verified yet.

### PanVK Android linker namespace 修复与 Xiaomi 11T 冒烟测试（2026-11-03）

安装版日志确认启动失败点为：`PanVK dlopen: library "libhardware.so" not found ... in namespace clns-4`。该库存在于设备上，但不在 Android 应用 linker namespace 的公开库列表内；这不是 PanVK 校验和或 Mali-G77 检测失败。PanVK 还带有 Termux 专用 `RUNPATH`；把 `libhardware.so` 复制进 APK 既不安全也不具备可移植性。

APK 现包含使用固定版本 `liblinkernsbypass` 构建的 `libpanvk_namespace.so`：它以应用私有 PanVK 目录创建共享 linker namespace，再链接到 Android 默认系统库并按 basename 加载 PanVK ICD；不会把 Android 私有库复制进 APK。PanVK 仍严格启动，System 仍是显式回退。

Xiaomi 11T 验证：`0.6.67` APK 使用已安装应用匹配的签名密钥覆盖安装，保留应用数据。以应用 UID 运行的严格 PanVK `gpu-driver-properties` 冒烟测试报告 ICD interface 7、`Mali-G77 MC9`、driver ID 20、Vulkan 1.3.366、subgroup 16；instance/device/queue 初始化和 `vkDeviceWaitIdle` 均完成（`GPU driver properties and dispatch PASS`）。
同一 app-UID 探针在不设置自定义驱动配置时选择系统驱动（ID 9），device create 与 idle 也通过。

此测试只验证 ICD 加载与设备初始化，不验证 Android surface/swapchain/present 或 D2R 实战。测试时手机仍在锁屏界面；启动器保存的选择仍为 `system`，因此不能宣称 PanVK 游戏兼容性已验证。

### JimVulkan PanVK r8 performance update (2026-11-03)

The FristOneRR v3.7.0 binary previously bundled here was replaced by JimVulkan's Mesa 27 r8 build. Its release notes specifically report a Mali-G77 2,000-small-draw benchmark improving from 6.55 ms to 2.2 ms (Arm's driver: 2.18 ms), matching the draw-heavy pattern in the captured D2R frames. This is an upstream microbenchmark, not a measured D2R/Xiaomi 11T result.

The APK still exposes the same three choices—System, PanVK, Turnip—and keeps PanVK strict; no queue waits or guest synchronization were weakened. PanVK is now gated to Android API 31+ because that is the packaged ICD's declared minimum.

### Xiaomi 11T 上 PanVK r8 的实际启动结果（2026-11-04）

APK `0.6.69` 真机测试确认驱动选择器可在 PanVK 与 System Vulkan 间切换，强制停止并重启后仍保存为 PanVK。点击启动后，游戏在进入渲染前中止：

```text
selected instance has no vkCreateAndroidSurfaceKHR
required driver/subgroup policy failed
```

以应用 UID 对安装版 ICD 运行独立探针：枚举到 14 个 instance extension（包含 `VK_KHR_surface` 和 `VK_KHR_android_surface`）；同时启用这两个扩展后 `vkCreateInstance` 成功，但 `vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR")` 返回 NULL。`vkDestroySurfaceKHR` 与 `vkGetPhysicalDeviceSurfaceSupportKHR` 非 NULL。因此这是 PanVK Android surface 入口缺失，不是驱动下拉框选错，也不是 linker namespace 未加载。仅列出 Android surface 扩展并不代表实现了 surface 创建。

启动器对 PanVK 使用 `require_custom=1`，所以不会静默换到系统驱动；测试没有验证 System 模式能否进入游戏，也没有实战 FPS 数据。当前 PanVK 在此设备上不能用于游戏；必须补齐真实 Android window/surface/swapchain/present 支持后再重新验证。

### PanVK r9 Android-loader / WSI dispatch smoke（2026-11-04）

APK `0.6.71` 使用已安装应用匹配的 `apk/d2r-debug.keystore` 构建并通过 `adb install -r` 原位安装。以应用 UID 运行严格 `require_custom=1` 的 driver-properties fixture，确认实际加载的是 PanVK（`driverID=20`），设备为 Mali-G77 MC9（vendor `0x13b5`、device `0x90800011`），API `1.4.363`、subgroup 16；`vkCreateDevice`、queue 与 `vkDeviceWaitIdle` 成功。`VK_KHR_surface`、`VK_KHR_android_surface` 和 `VK_KHR_swapchain` 可用，Android-surface 与 swapchain 分发表检查通过。System Vulkan 对照（driver ID 9、API `1.1.177`）也通过同一 WSI dispatch smoke。

这次只测 Vulkan loader、PanVK 身份、设备创建和 WSI dispatch；没有创建真实 `ANativeWindow` surface、swapchain、present 帧，也没有启动 D2R 或测 FPS。PanVK 仍标记为非一致性实验驱动；游戏实测由用户进行。
