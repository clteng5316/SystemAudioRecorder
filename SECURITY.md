# Security Policy

## Supported versions

SystemAudioRecorder is currently a pre-release project.

| Version | Supported |
|---|---|
| `v2.0-beta.1` | Yes |
| Older or unpublished builds | No |

Security fixes, when necessary, will normally be made against the current release line.

## Reporting a vulnerability

Please **do not report security vulnerabilities in a public GitHub Issue**.

Use GitHub's **Private vulnerability reporting** feature for this repository so that the report can be reviewed privately before public disclosure.

When reporting a vulnerability, please include as much of the following information as possible:

- affected SystemAudioRecorder version;
- affected edition: Windows x64 GUI, Linux x64 CLI, or Linux x64 GUI;
- operating system and version;
- clear description of the issue and its potential security impact;
- steps required to reproduce the issue;
- relevant logs, screenshots, or proof-of-concept details, if appropriate;
- any suggested mitigation or fix, if known.

Please remove passwords, authentication cookies, session tokens, API keys, private URLs, personal data, and other unrelated sensitive information from reports and attachments.

## Disclosure

Please allow reasonable time for the issue to be investigated and, when appropriate, corrected before publicly disclosing vulnerability details.

A report may be closed without a security fix if it cannot be reproduced, does not affect SystemAudioRecorder, depends entirely on unsupported third-party software, or does not represent a security vulnerability.

## Third-party components

SystemAudioRecorder may invoke external tools such as FFmpeg, yt-dlp, and platform audio utilities. Vulnerabilities that originate entirely in those third-party projects should normally be reported to their respective maintainers.

If a SystemAudioRecorder integration with a third-party component creates or exposes a security issue specific to this project, it may still be reported here.

---

## 安全性政策（繁體中文）

### 支援版本

SystemAudioRecorder 目前為預發佈專案。

| 版本 | 是否支援 |
|---|---|
| `v2.0-beta.1` | 是 |
| 舊版或未公開版本 | 否 |

如有需要，安全性修正原則上會針對目前的發佈版本線進行。

### 回報安全漏洞

請**不要在公開的 GitHub Issue 中回報安全漏洞**。

請使用本 Repository 的 GitHub **Private vulnerability reporting（私密漏洞回報）**功能，讓問題可以在公開揭露前先進行私下檢查與處理。

回報時請盡可能提供：

- 受影響的 SystemAudioRecorder 版本；
- 受影響的版本類型：Windows x64 GUI、Linux x64 CLI 或 Linux x64 GUI；
- 作業系統與版本；
- 問題內容及可能造成的安全影響；
- 可重現問題的操作步驟；
- 適當的紀錄、截圖或概念驗證資訊；
- 若已知，可提供可能的緩解方式或修正建議。

請先移除密碼、登入 Cookie、Session Token、API Key、私人網址、個人資料及其他與回報無關的敏感資訊，再上傳紀錄或附件。

### 漏洞揭露

在公開漏洞細節前，請預留合理時間讓問題進行調查，並在適當情況下完成修正。

若問題無法重現、並非由 SystemAudioRecorder 造成、完全源自不支援的第三方軟體，或不屬於安全漏洞，回報可能會在不進行安全修正的情況下結案。

### 第三方元件

SystemAudioRecorder 可能呼叫 FFmpeg、yt-dlp 及作業系統音訊工具等外部程式。若漏洞完全源自這些第三方專案，原則上應回報給相對應的維護者。

若 SystemAudioRecorder 與第三方元件的整合方式造成或暴露本專案特有的安全問題，仍可透過本 Repository 回報。
