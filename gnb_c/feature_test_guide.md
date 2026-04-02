# gnb_c 当前功能与测试指南

本文档整理当前 `gnb_c` 中基站、UE 和核心网桥接已经完成的功能，以及对应的测试方法。

## 1. 当前已经完成的功能

### 1.1 本地 UE <-> gNB 文件系统闭环

当前已经实现了一个基于本地 JSON 事件目录的单 UE 闭环：

- `apps/mini_ue_c.c`
  - 根据 `config/default_cell.yml` 中的时序配置，生成单 UE 事件序列
  - 将事件写入 `sim.local_exchange_dir/ue_to_gnb/`
- `apps/mini_gnb_c_sim.c`
  - 从 `sim.local_exchange_dir/ue_to_gnb/` 读取 UE 事件
  - 将这些事件映射到现有 mock PHY/MAC 上行输入
  - 在 slot 驱动模型中完成随机接入和后续简化调度

当前已经覆盖的本地流程为：

- `PRACH -> RAR -> MSG3 -> MSG4`
- `PUCCH_SR`
- `BSR`
- `UL DATA`

运行结束后，可以在 `out/summary.json` 中看到提升后的 UE 上下文，以及内嵌的 `core_session` 基础状态。

### 1.2 gNB -> AMF 的最小控制面桥接

当前 simulator 内部已经接入一个最小 `gNB -> AMF` bridge：

- gNB 可以建立到 AMF 的 SCTP/NGAP 连接
- 可以完成 `NGSetup`
- 可以在 UE promote 后发送第一条 `InitialUEMessage`
- 可以接收第一条 `DownlinkNASTransport`
- 可以继续轮询 `ue_to_gnb_nas/*.json` 中的 `UL_NAS` 事件，并将其转发为 `UplinkNASTransport`
- 可以把 AMF 返回的后续 `DL_NAS` 写入 `gnb_to_ue/*.json`

### 1.3 会话建立状态解析

当前 bridge 已经具备最小 session setup 解析能力：

- 识别 `InitialContextSetupRequest`
- 自动回 `InitialContextSetupResponse`
- 识别 `PDUSessionResourceSetupRequest`
- 自动回 `PDUSessionResourceSetupResponse`
- 从 AMF 下发消息中解析并写入 `core_session`

当前已解析字段包括：

- `ran_ue_ngap_id`
- `amf_ue_ngap_id`
- `ue_ipv4`
- `upf_ip`
- `upf_teid`
- `qfi`
- NAS 上下行计数

这些状态会体现在 `out/summary.json` 里。

### 1.4 已有的 Open5GS 外部验证工具

仓库里另外还有一个独立工具：

- `apps/ngap_probe.c`

它不是 `mini_ue_c + mini_gnb_c_sim` 这条路径的一部分，但它已经可以：

- 进行一次性 `NGSetupRequest` 连通性探测
- 在 `--replay` 模式下走完整的 Open5GS 控制面回放
- 解析会话建立得到的 `UE IPv4 / UPF IP / TEID / QFI`
- 对 UPF 发送最小 GTP-U Echo 和 G-PDU

当前仓库内置的参考抓包位于：

- `examples/gnb_ngap.pcap`
- `examples/gnb_mac.pcap`

## 2. 当前还没有完成的功能

以下能力尚未完成，因此目前不能认为系统已经具备完整端到端 UE 仿真：

- `mini_ue_c` 还不会读取 `gnb_to_ue/*.json` 后自动生成后续 UE NAS 响应
- 还没有在 UE 侧实现完整的 NAS 状态机
- 还没有完成持久化 N3 用户面
- 还没有完成 TUN 接口接入
- 还没有完成 `server -> UPF -> gNB -> UE` 的真实 `ping` 闭环

## 3. 基础构建与回归测试

先执行完整构建和测试：

```bash
cd /home/hzy/codex/test2/codex/gnb_c
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

如果想看所有单元测试和集成测试逐项通过的名称，可以直接运行测试二进制：

```bash
cd /home/hzy/codex/test2/codex/gnb_c
./build/mini_gnb_c_tests
```

当前与 UE/gNB/core bridge 关系最直接的测试包括：

- `test_integration_local_exchange_ue_plan`
- `test_integration_core_bridge_prepares_initial_message`
- `test_integration_core_bridge_relays_followup_ul_nas`
- `test_integration_core_bridge_extracts_session_setup_state`
- `test_gnb_core_bridge_prepares_initial_ue_message`
- `test_gnb_core_bridge_relays_followup_uplink_nas`
- `test_gnb_core_bridge_parses_session_setup_state`

## 4. 测试本地 UE <-> gNB 闭环

### 4.1 测试目标

验证当前 `mini_ue_c` 和 `mini_gnb_c_sim` 是否能够通过本地 JSON 目录完成单 UE 闭环。

### 4.2 测试命令

```bash
cd /home/hzy/codex/test2/codex/gnb_c
rm -rf out/local_exchange out/summary.json
./build/mini_ue_c config/default_cell.yml
./build/mini_gnb_c_sim config/default_cell.yml
```

### 4.3 预期结果

`mini_ue_c` 会生成一组 JSON 事件，例如：

- `out/local_exchange/ue_to_gnb/seq_000001_ue_PRACH.json`
- `out/local_exchange/ue_to_gnb/seq_000002_ue_MSG3.json`
- `out/local_exchange/ue_to_gnb/seq_000003_ue_PUCCH_SR.json`

`mini_gnb_c_sim` 运行结束后，应生成：

- `out/summary.json`

可以进一步检查：

```bash
ls out/local_exchange/ue_to_gnb
sed -n '1,240p' out/summary.json
```

在当前阶段，`summary.json` 中应至少体现：

- UE 已被 promote
- `rrc_setup_sent=true`
- `pucch_sr_detected=true`
- `ul_bsr_received=true`
- `ul_data_received=true`

## 5. 测试 gNB 到 AMF 的第一跳桥接

### 5.1 测试目标

验证 simulator 在启用 `core.enabled` 后，是否能够：

- 连到 AMF
- 完成 `NGSetup`
- 在 UE promote 后发送第一条 `InitialUEMessage`
- 接收第一条 `DL_NAS`

### 5.2 准备配置

复制默认配置并临时打开核心网桥接：

```bash
cd /home/hzy/codex/test2/codex/gnb_c
cp config/default_cell.yml /tmp/mini_gnb_core.yml
sed -i 's/^  enabled: false$/  enabled: true/' /tmp/mini_gnb_core.yml
```

如果你的 Open5GS AMF 地址不是默认值，也可以继续修改：

- `core.amf_ip`
- `core.amf_port`
- `core.timeout_ms`

### 5.3 测试命令

```bash
cd /home/hzy/codex/test2/codex/gnb_c
rm -rf out/local_exchange out/summary.json
./build/mini_ue_c /tmp/mini_gnb_core.yml
./build/mini_gnb_c_sim /tmp/mini_gnb_core.yml
```

### 5.4 预期结果

如果 AMF 可达，当前应能看到：

- `out/local_exchange/gnb_to_ue/seq_000001_gnb_DL_NAS.json`
- `out/summary.json` 中出现有效的 `ran_ue_ngap_id`
- `out/summary.json` 中出现有效的 `amf_ue_ngap_id`

可以直接检查：

```bash
ls out/local_exchange/gnb_to_ue
sed -n '1,240p' out/local_exchange/gnb_to_ue/seq_000001_gnb_DL_NAS.json
sed -n '1,240p' out/summary.json
```

注意：当前这一步只验证第一跳控制面闭环，不代表完整 attach 已自动完成。

## 6. 测试 follow-up UL_NAS / DL_NAS 桥接

### 6.1 测试目标

验证 gNB bridge 是否会继续轮询 `ue_to_gnb_nas/` 并将后续 UE NAS 转发给 AMF。

### 6.2 当前限制

当前 `mini_ue_c` 还不会自动根据 `DL_NAS` 生成后续 `UL_NAS`。因此这一步目前主要依赖测试代码，或者你手工写入事件文件。

### 6.3 手工事件格式

当前支持的 `UL_NAS` 事件格式示例：

```json
{
  "sequence": 1,
  "abs_slot": 7,
  "channel": "ue_to_gnb_nas",
  "source": "ue",
  "type": "UL_NAS",
  "payload": {
    "c_rnti": 17921,
    "nas_hex": "7E005C000D0164F099F0FF00002143658789"
  }
}
```

文件名格式为：

```text
out/local_exchange/ue_to_gnb_nas/seq_000001_ue_UL_NAS.json
```

你手工补入这样的事件后，simulator 会在对应 slot 到达时转发。

### 6.4 推荐验证方式

这一阶段最稳的回归方式仍然是执行已有测试：

```bash
cd /home/hzy/codex/test2/codex/gnb_c
./build/mini_gnb_c_tests
```

重点看：

- `test_integration_core_bridge_relays_followup_ul_nas`
- `test_gnb_core_bridge_relays_followup_uplink_nas`

## 7. 测试 session setup 状态解析

### 7.1 测试目标

验证 bridge 在收到后续 AMF session setup 消息后，是否已经能够：

- 自动回 `InitialContextSetupResponse`
- 自动回 `PDUSessionResourceSetupResponse`
- 解析 `ue_ipv4`
- 解析 `upf_ip`
- 解析 `upf_teid`
- 解析 `qfi`

### 7.2 推荐测试命令

```bash
cd /home/hzy/codex/test2/codex/gnb_c
./build/mini_gnb_c_tests
```

重点看：

- `test_integration_core_bridge_extracts_session_setup_state`
- `test_gnb_core_bridge_parses_session_setup_state`
- `test_ngap_runtime_extracts_open5gs_user_plane_state`

### 7.3 预期结果

测试通过后，说明 simulator 侧已经具备 Stage C5 的最小 session state 提取能力。对应状态会导出到：

- `out/summary.json`

相关字段包括：

- `upf_ip`
- `upf_teid`
- `qfi`
- `ue_ipv4`

## 8. 测试独立的 Open5GS 外部验证工具

### 8.1 最小 N2 连通性探测

```bash
cd /home/hzy/codex/test2/codex/gnb_c
./build/ngap_probe 127.0.0.5 38412 5000
```

成功时应看到：

- `NGSetupResponse detected.`

### 8.2 完整 replay 验证

```bash
cd /home/hzy/codex/test2/codex/gnb_c
./build/ngap_probe --replay --upf-ip 127.0.0.7 --upf-port 2152 127.0.0.5 38412 5000
ip -s link show dev ogstun
```

这一步主要用于验证：

- AMF 控制面连通
- Open5GS attach/session 流程回放
- UPF GTP-U 连通

默认情况下，`--replay` 会使用仓库内的参考 N2 抓包：

- `examples/gnb_ngap.pcap`

运行后通常会看到生成物：

- `out/ngap_probe_ngap_runtime.pcap`
- `out/ngap_probe_gtpu_runtime.pcap`

## 9. 当前测试结论应该怎么理解

如果以下三类测试都通过：

- `ctest --test-dir build --output-on-failure`
- 本地 `mini_ue_c -> mini_gnb_c_sim` 闭环
- `ngap_probe --replay`

那么当前可以认为：

- 本地简化 UE 与 gNB 的 JSON 驱动闭环已经成立
- gNB 到 AMF 的最小控制面桥接已经成立
- simulator 侧已经能提取会话建立得到的关键 session state

但还不能认为：

- `mini_ue_c` 已具备完整 UE NAS 行为
- gNB 与 UE 已具备真实用户面收发
- 已经实现 `server -> UPF -> gNB -> UE` 的最终 `ping` 闭环
