[English](AGENT.md)

# Agent 执行手册：一键接手心力阅读器

用户把此包交给你，表示授权准备安装与配置。实际写设备前，确认设备、此次固件及是否替换原设置；不要默认同意擦除。支持macOS、ESP32-C3 8MB。本文中的命令均从解压目录运行。不要在原制作者电脑上安装朋友的服务。

## 1. 检查并准备

读 README.zh_CN.md。执行 `python3 install.py --verify`、`python3 install.py --prepare`。首次需要联网下载requirements.txt中的依赖；不安装系统级Python包，不安装ESP-IDF。若Python或权限缺失，明确说明所需本机操作，不申请心力球密码/token。

执行 `python3 install.py --devices`，让用户通过拔插识别唯一AI Passport序列号。存在Dot或其他ESP32时也不操作它们。不要把本包作者的设备身份当朋友的身份。

## 2. 获授权后安装

默认保留设置（只有现有分区完全兼容才会继续）：

```bash
python3 install.py --install --serial '<朋友已确认的序列号>' --approve-flash
```

首次从原厂或其他应用切换，分区可能不兼容。说明更换固件与Wi-Fi等设置的数据影响；用户明确同意全新安装后才执行：

```bash
python3 install.py --install --serial '<朋友已确认的序列号>' --approve-flash --fresh-install
```

脚本再次确认芯片和8MB容量，只向该串口写验证固件；保存朋友自己的设备绑定，创建独立密钥，安装用户级登录服务，并打开配网页面。全新模式把full.bin写至0x0，分段模式写0x0/0x8000/0x10000且避开NVS。不要自己改用应用镜像刷0x0，不运行全片擦除。检测到已有安装或8765端口占用时停止，检查归属后再给迁移方案，禁止覆盖或停掉其他任务。

Wi-Fi让用户在本机 `http://127.0.0.1:8765/setup` 填写并保存。自动采用本机Bonjour名称连接局域网。不要把密码填进对话、脚本文件或日志；首次设置需USB，日常更新只用Wi-Fi。同网、2.4GHz与Bonjour解析不通时排查网络隔离，不擅改防火墙或电源。

## 3. 心力球来源与首次真实内容

发现并使用你当前环境支持的官方心力球MCP连接方式，由朋友授权他自己的账号。必须实际调用 recall_memory 与 recall_note，不能把安装连接器视为读取成功。若工具未挂载/授权失败，停止内容发布并报告，不改用制作者的凭证或示例句子。

运行 `.venv/bin/python tools/quote_window.py --prepare`。只处理 update=true 的 issue_date/start_date/date/covered_days/missing_days；假日直接退出。首次安装无旧日桶，需要查完所有 missing_days。以后仅补新增日期，不重排已有选句。当前只维护2026年假日；未知年份需依据官方已发布安排核实、测试与同步服务，不能猜测。

逐日同时检索全时记和随口记并翻完所有页。北京时间边界换毫秒，左开右闭：日00:00减1毫秒，至次日00:00减1毫秒。优先会话摘要，再按需核实原句及时间；工具返回很大时先在编排层筛选/本地落盘，不把完整转写灌进模型上下文。区分本人、他人、外放节目和AI对话，不都写成朋友亲口说的。

每日至多5条，以3条为宜；有价值、忠于原意、独立可读。正文text建议≤32字（硬上限36）、source建议≤8字（硬上限10），统一明确标为录音提炼，保留真实day和HH:MM。排除隐私标识、单位内部信息和未经核实的医疗断言。仅用prepare_quotes.inventory支持字符；没有合适内容宁可少选。

生成私有候选JSON，格式如下（占位值必须替换，不可直接发布）：

```json
{"date":"<窗口截止日>","reviewed_days":["<完整查阅的缺失日>"],"quotes":[{"day":"<来源日>","text":"<录音提炼正文>","source":"<准确标注>","time":"<真实HH:MM>"}]}
```

reviewed_days只能列本次完整检查过的缺失日期。查完无可选句子可列日期但不添句，授权失败或漏页不能算已查。原始记录、候选及来源证据仅留private目录。

发布：

```bash
.venv/bin/python tools/publish_quotes.py 'private/<候选文件>.json' --issue-date '<计划发布日>'
```

发布器校验候选，保留已有日桶，按日拼接去重，原子写入private/editions及Mac运行副本。不要直接用prepare_quotes.py校验候选，二者格式不同。成品才可再用 `.venv/bin/python tools/prepare_quotes.py 'private/editions/<发布日>.json'` 校验。成品最多48条/9000字节；7天各5条最多35。整期没有可用句子或校验失败时不清空旧期。

## 4. 验收与定时

在本机通过鉴权访问 /v1/quotes、/v1/context。从private/device-key本地读取Bearer，不打印；只输出期号、起止日、条数、covered_days、农历天数和是否有天气。09:35前仍返回上一期正常，可用mac_bridge.select_pack传入当天09:35检查暂存内容，不能提前释放。

让朋友确认屏幕中文、时钟/农历/天气、三键和10秒轮播；首次无内容显示待同步正常。只有实际导入/联网获取或用户确认才记设备成功。

在本Agent中创建唯一的北京时间工作日09:25任务：读计划、跳过假日、查两类心力记录全部页、只提炼missing_days、校验发布、确认服务。服务09:35开放，设备随后一分钟拉取。规则：周一完整上周，周二本周一，周三本周一至周二，周四本周一至周三，周五本周一至周四。周一放假首工作日先补上周，下一工作日开始本周累积。所有周末不更新，包括调休周末。

任务必须实际创建并核验时区/下次运行时间；如果本Agent没有定时能力，明确列待完成，不拿文档代替完成。保持Mac醒着且Agent能运行，验证退出/休眠影响。成功安静，失败或需用户操作才通知。不额外开付费服务，不创建Codex/其他Agent重复任务。

## 5. 数据位置、恢复与报告

原解压目录private保存本人的日桶、成品、设备绑定和密钥；运行副本在 `~/Library/Application Support/AIPassportHeartBridge`，启动项 `local.ai-passport.heart-bridge`。服务使用解压目录的.venv，不移动删除该目录。现有服务未运行才用 `.venv/bin/python tools/install_mac_bridge.py` 恢复，日历更新后也用它同步，不另开同端口服务。

失败保留旧内容；不清空Wi-Fi/日桶/密钥，不上传整个private。卸载执行 `.venv/bin/python tools/install_mac_bridge.py --uninstall` 并取消你创建的定时任务，不恢复原厂固件。

最终分别报告：安装包校验、依赖准备、固件写入、本人账号读取、首次发布、屏幕与按键、定时任务。另列未验证的定时触发、跨周、节假日及续航。共享固件构建通过不代表朋友设备已通过验收。
