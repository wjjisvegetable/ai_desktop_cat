# 桌面招财猫：云端对话 API 配置

本版本的连接链路是：**手机 App → 同一局域网内的电脑代理 → OpenAI Responses API**。API key 只放在电脑上。手机 App 的“代理令牌”是你自己设置的局域网访问口令，**不是** OpenAI API key。

## 1. 准备 API key

在 [OpenAI API 平台](https://platform.openai.com/api-keys)创建项目 API key，并确认对应项目可使用所选模型。只登录当前 ChatGPT 账号，不能让此 App 自动读取账号或本次聊天记录；即使使用同一个登录邮箱，也需要为这个代理配置 API 凭据。不要把 key 写进 Android 源码、APK、截图或 GitHub 仓库。参见 [OpenAI 官方 Quickstart](https://developers.openai.com/api/docs/quickstart)和[API 凭据安全说明](https://developers.openai.com/api/reference/overview)。

## 2. 在电脑上启动代理

安装 Node.js **20.6 或更高版本**。在终端进入本仓库的 `voice-proxy/` 文件夹，复制示例配置：

```sh
cp .env.example .env
chmod 600 .env
```

用文本编辑器修改 `.env`。下面是字段示例，尖括号内容必须换成你自己的值：

```dotenv
OPENAI_API_KEY=<刚创建的API密钥>
OPENAI_MODEL=gpt-4.1-mini
DESKTOPCAT_PROXY_TOKEN=<至少16字符的随机口令>
DESKTOPCAT_HOST=0.0.0.0
DESKTOPCAT_PORT=8787
```

可在终端运行 `openssl rand -hex 24` 生成 48 字符的代理令牌，把输出复制到 `.env` 的 `DESKTOPCAT_PROXY_TOKEN`，稍后也要填进手机 App。`.env` 已被仓库的 `.gitignore` 排除，上传 GitHub 前仍应检查它没有出现在提交列表中。

仍在 `voice-proxy/` 目录运行：

```sh
node --env-file=.env server.mjs
```

保持这个终端运行。另开终端检查 `curl http://127.0.0.1:8787/health`，应得到包含 `"ok":true` 和 `"cloudConfigured":true` 的 JSON。此检查只证明代理已读取 key，尚未证明云端请求成功。

## 3. 在手机 App 中填写

电脑和手机连接同一个可信 Wi-Fi。先在电脑上运行 `ipconfig getifaddr en0` 查看 Wi-Fi IPv4 地址；若没有结果，在系统网络设置中查看本机 IPv4。假设结果是 `192.168.1.20`，就在 App 的 **对话 → 代理设置** 中填写：

| 字段 | 示例 |
|---|---|
| 电脑代理地址 | `http://192.168.1.20:8787` |
| 代理令牌 | `.env` 中的 `DESKTOPCAT_PROXY_TOKEN` 原文 |

**不要把 `OPENAI_API_KEY` 填进 App。**输入一句“你好”并点击发送，即可验证真实云端调用。手机语音输入使用手机系统识别服务；识别出文字后需再点击发送。回复可由手机 TTS 朗读。最近 20 条对话保存在手机本机，并在后续提问时作为上下文发送。

如果还要同时控制桌面猫，建议手机保持家庭 Wi-Fi 以访问电脑代理，使用 **BLE** 连接 S3。若手机改连 S3 自带的 `DesktopCat` Wi-Fi 热点，通常就无法再访问家庭局域网中的电脑代理。首次校准和启动舵机时，可以临时切回 S3 热点使用 Wi-Fi 控制页，完成后再回家庭 Wi-Fi 并用 BLE 控制。

## 4. 常见问题

| 现象 | 检查项 |
|---|---|
| App 报“只能访问私有局域网地址” | 只填电脑的 `10.x.x.x`、`172.16–31.x.x` 或 `192.168.x.x` 地址，不填公网地址、域名或 `localhost`。 |
| 无法访问代理或超时 | 确认电脑代理终端还在运行、手机与电脑同网、电脑防火墙允许 8787 端口，并检查电脑 IPv4 是否变化。 |
| 返回 401 | App 中的代理令牌与 `.env` 的 `DESKTOPCAT_PROXY_TOKEN` 不一致。 |
| 返回“云端请求失败 (401)” | 检查电脑端的 OpenAI API key 是否正确。 |
| 返回“云端请求失败 (429)” | 在 API 平台检查项目用量、额度和速率限制。 |
| `cloudConfigured:false` | 代理没有读取到 `OPENAI_API_KEY`，检查 `.env` 位置和启动命令。 |

当前代理使用局域网 HTTP，适合可信实验网络；不要在路由器上开放 8787 公网端口。对外访问需另行配置 HTTPS 和访问控制。C5 板载麦克风、扬声器尚未接入本云端链路。
