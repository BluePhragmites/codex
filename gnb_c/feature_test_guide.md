# gnb_c 当前功能与测试指南

本文档整理当前 `gnb_c` 中基站、UE 和核心网桥接已经完成的功能，以及对应的测试方法。

## 1. 当前已经完成的功能

### 1.1 本地 UE <-> gNB 共享寄存器闭环

当前默认推荐的本地 UE/gNB 联动方式已经切换成共享寄存器式的 shared-slot 闭环：

- `apps/mini_ue_c.c`
  - 在配置了 `sim.shared_slot_path` 时，不再预生成一整批 JSON 事件
  - 而是进入 live runtime，逐 slot 读取 gNB 发布的 `txSlot` 状态，并写回 UE 自己的 `rxSlot` 进度和未来上行事件
  - UE 可以使用和 gNB 不同的配置文件；接入和连接态时机不再要求两边事先写死一致
- `apps/mini_gnb_c_sim.c`
  - 每个 slot 开始时先读 UE 是否为该 slot 留下了上行事件
  - 每个 slot 结束时将本 slot 的下行摘要写入 shared-slot 寄存器，然后等待 UE 确认读取
- `include/mini_gnb_c/link/shared_slot_link.h`
  - 封装了这组共享内存寄存器
  - gNB 侧只推进 `txSlot`
  - UE 侧只推进 `rxSlot`
  - 双方通过一个 DL 槽摘要和一个待消费 UL 事件完成联动

当前已经覆盖的本地流程为：

- `PRACH -> RAR -> MSG3 -> MSG4`
- `PUCCH_SR`
- `BSR`
- `UL DATA`

当前 live UE runtime 的时机来源已经改成“由下行消息学习”：

- `SIB1` 的 `PDSCH` 负载里带 `prach_period_slots / prach_offset_slot / ra_resp_window / prach_retry_delay_slots`
- UE 只会在这些 `SIB1` 宣告的 PRACH 时机上发 `Msg1`
- `SIB1` 还带 gNB 配置出来的 `dl_time_indicator / dl_data_to_ul_ack_slots / ul_time_indicator / dl_harq_process_count / ul_harq_process_count`
- 如果这些字段没有在 gNB YAML 中显式配置，配置加载器会补默认值，再由 gNB 继续通过 `SIB1` 广播给 UE
- `RAR` 里带 mock `Msg3` 的绝对 slot
- `Msg4` 的 `RRCSetup` 里带 `sr_period_slots / sr_offset_slot`
- UE 只会在这些 `Msg4` 宣告的 SR 时机上发 `PUCCH_SR`

运行结束后，可以在 `out/summary.json` 中看到提升后的 UE 上下文，以及内嵌的 `core_session` 基础状态。

### 1.2 本地 JSON 目录现在只保留给控制面和回归

原先的 `sim.local_exchange_dir/ue_to_gnb/*.json` 方案没有完全删除，但它已经不再是主路径：

- 对 radio 路径来说，它只保留为回归测试和兼容模式
- 对 core bridge 来说，`sim.local_exchange_dir` 仍然用于：
  - `ue_to_gnb_nas/*.json`
  - `gnb_to_ue/*.json`

也就是说，现在的职责已经分开：

- live radio loop: `shared_slot_path`
- follow-up NAS event handoff: `local_exchange_dir`

### 1.3 gNB -> AMF 的最小控制面桥接

当前 simulator 内部已经接入一个最小 `gNB -> AMF` bridge：

- gNB 可以建立到 AMF 的 SCTP/NGAP 连接
- 可以完成 `NGSetup`
- 可以在 UE promote 后发送第一条 `InitialUEMessage`
- 可以接收第一条 `DownlinkNASTransport`
- 可以继续轮询 `ue_to_gnb_nas/*.json` 中的 `UL_NAS` 事件，并将其转发为 `UplinkNASTransport`
- 可以把 AMF 返回的后续 `DL_NAS` 写入 `gnb_to_ue/*.json`

### 1.4 会话建立状态解析

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

### 1.5 已有的 Open5GS 外部验证工具

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

- `mini_ue_c` 的 live runtime 还不会读取 `gnb_to_ue/*.json` 后自动生成后续 UE NAS 响应
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

- `test_shared_slot_link_round_trip`
- `test_shared_slot_link_handles_slot_zero_and_shutdown_boundaries`
- `test_integration_shared_slot_ue_runtime`
- `test_integration_core_bridge_prepares_initial_message`
- `test_integration_core_bridge_relays_followup_ul_nas`
- `test_integration_core_bridge_extracts_session_setup_state`
- `test_gnb_core_bridge_prepares_initial_ue_message`
- `test_gnb_core_bridge_relays_followup_uplink_nas`
- `test_gnb_core_bridge_parses_session_setup_state`

## 4. 测试本地 UE <-> gNB shared-slot 闭环

### 4.1 测试目标

验证当前 `mini_ue_c` 和 `mini_gnb_c_sim` 是否能够通过共享寄存器式的 shared-slot 完成单 UE 实时联动，并且 UE 通过 gNB 下发的 `SIB1 / RAR / Msg4` 学到接入与 SR 时机。

### 4.2 测试命令

```bash
cd /home/hzy/codex/test2/codex/gnb_c
rm -rf out/local_exchange out/shared_slot_link.bin out/summary.json
./build/mini_ue_c config/example_shared_slot_ue.yml &
UE_PID=$!
./build/mini_gnb_c_sim config/example_shared_slot_gnb.yml
wait $UE_PID
```

### 4.3 预期结果

预期现象：

- UE 进程会打印当前已经排入 future slot 的上行事件，例如 `UE scheduled PRACH for abs_slot=2`
- UE 配置文件不需要重复写 gNB 的 PDCCH/HARQ 时序参数；这些由 gNB 配置并经 `SIB1` 下发
- gNB 进程会在日志中看到实际收到的 `PRACH / MSG3 / PUCCH_SR / DATA`
- 会生成 `out/shared_slot_link.bin`
- `out/summary.json`

可以进一步检查：

```bash
sed -n '1,240p' out/summary.json
```

在当前阶段，`summary.json` 中应至少体现：

- UE 已被 promote
- `rrc_setup_sent=true`
- `pucch_sr_detected=true`
- `ul_bsr_received=true`
- `ul_data_received=true`

推荐额外检查 UE 日志，确认它是按 gNB 下行信息学时机：

- 先看到 `UE scheduled PRACH for abs_slot=2`
- 在收到 `RAR` 后看到 `UE scheduled MSG3 for abs_slot=6`
- 在收到 `Msg4` 后看到 `UE scheduled PUCCH_SR for abs_slot=12`

### 4.4 shared-slot 边界语义

当前实现已经显式覆盖了你提出的两个边界：

- 起始边界
  - `txSlot` 初始为 `-1`
  - gNB 在 slot 0 先发布，UE 只读取并确认
- 结束边界
  - UE 可以在结束前留下最后一个 future UL 事件
  - gNB 即使在 UE 标记结束后，也仍可把这个最终 UL 事件读走

对应自动测试：

- `test_shared_slot_link_handles_slot_zero_and_shutdown_boundaries`

## 5. 测试 legacy JSON UE 计划回退路径

### 5.1 测试目标

验证旧的 `ue_to_gnb/*.json` 回退路径仍然可用，避免 shared-slot 改造后把已有回归手段破坏掉。

### 5.2 测试命令

```bash
cd /home/hzy/codex/test2/codex/gnb_c
rm -rf out/local_exchange out/summary.json
./build/mini_ue_c config/default_cell.yml
./build/mini_gnb_c_sim config/default_cell.yml
```

### 5.3 预期结果

`mini_ue_c` 会重新生成：

- `out/local_exchange/ue_to_gnb/seq_000001_ue_PRACH.json`
- `out/local_exchange/ue_to_gnb/seq_000002_ue_MSG3.json`
- `out/local_exchange/ue_to_gnb/seq_000003_ue_PUCCH_SR.json`

这个模式主要用于旧回归和局部调试，不再是推荐的主交互方式。

## 6. 测试 gNB 到 AMF 的第一跳桥接

### 6.1 测试目标

验证 simulator 在启用 `core.enabled` 后，是否能够：

- 连到 AMF
- 完成 `NGSetup`
- 在 UE promote 后发送第一条 `InitialUEMessage`
- 接收第一条 `DL_NAS`

### 6.2 准备配置

复制 shared-slot 示例配置并临时打开核心网桥接：

```bash
cd /home/hzy/codex/test2/codex/gnb_c
cp config/example_shared_slot_loop.yml /tmp/mini_gnb_core.yml
sed -i 's/^  enabled: false$/  enabled: true/' /tmp/mini_gnb_core.yml
```

如果你的 Open5GS AMF 地址不是默认值，也可以继续修改：

- `core.amf_ip`
- `core.amf_port`
- `core.timeout_ms`

### 6.3 测试命令

```bash
cd /home/hzy/codex/test2/codex/gnb_c
rm -rf out/local_exchange out/shared_slot_link.bin out/summary.json
./build/mini_ue_c /tmp/mini_gnb_core.yml &
UE_PID=$!
./build/mini_gnb_c_sim /tmp/mini_gnb_core.yml
wait $UE_PID
```

`/tmp/mini_gnb_core.yml` 同时提供：

- shared-slot radio 联动路径
- `local_exchange_dir` 下的 `DL_NAS/UL_NAS` 控制面事件目录

### 6.4 预期结果

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

## 7. 测试 follow-up UL_NAS / DL_NAS 桥接

### 7.1 测试目标

验证 gNB bridge 是否会继续轮询 `ue_to_gnb_nas/` 并将后续 UE NAS 转发给 AMF。

### 7.2 当前限制

当前 `mini_ue_c` 还不会自动根据 `DL_NAS` 生成后续 `UL_NAS`。因此这一步目前主要依赖测试代码，或者你手工写入事件文件。

### 7.3 手工事件格式

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

### 7.4 推荐验证方式

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
