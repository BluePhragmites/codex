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

### 1.5 Session setup 之后的后续控制面 relay

当前 control-plane bridge 不会在 `PDUSessionResourceSetupRequest` 之后停住：

- 它会继续处理后续 slot 上的 `ue_to_gnb_nas/*.json`
- 如果 AMF 在 session setup 之后继续发顶层 `DownlinkNASTransport`
- 这些后续 `DL_NAS` 仍会继续写入 `gnb_to_ue/*.json`
- 已提取的 `ue_ipv4 / upf_ip / upf_teid / qfi` 会继续保留在 `core_session` 中

另外，`ue_to_gnb_nas` 的时序语义现在也有明确覆盖：

- 早于当前 slot 的 stale `UL_NAS` 会被跳过
- 晚于当前 slot 的 future `UL_NAS` 会保留到对应 slot 再发送

### 1.6 持久化 N3 用户面 socket 基础层

当前已经完成 Stage D1 的传输层基础设施，但还没有完成完整用户面闭环：

- simulator 在 `core_session` 中拿到有效的：
  - `upf_ip`
  - `upf_teid`
  - `qfi`
  - `ue_ipv4`
- 之后会自动激活一个长期存在的 N3 UDP socket，而不是像 `ngap_probe` 那样只打一发 probe
- 这个 socket 的目标端口来自 gNB 配置里的 `core.upf_port`
- 当前 helper 已经支持：
  - 复用 `gtpu_tunnel` 构造上行 G-PDU
  - 查询本地绑定的 UDP endpoint
  - 在 slot loop 中非阻塞轮询下行 GTP-U
- 当前 simulator 已经会把合法的上行 IPv4 `UL DATA` 送进持久化 N3 socket，并把下行 GTP-U 解包后的 inner IPv4 负载重新排成 `DL_OBJ_DATA`

### 1.7 最小 UE 侧 IPv4/ICMP 用户面

当前已经完成 Stage D2 的最小用户面行为：

- 新增了 `ue_ip_stack_min`
- 当 UE 在 shared-slot runtime 中收到 `DL_OBJ_DATA` 时：
  - 如果载荷不是 IPv4，就忽略
  - 如果载荷是发给 UE 的 `ICMP Echo Request`，就构造并缓存一个 `ICMP Echo Reply`
- 当 UE 后续收到 `DCI0_1` 给出的 `UL DATA` grant 时：
  - 如果有 pending `ICMP Echo Reply`，优先把这个 reply 作为上行 PUSCH 载荷
  - 否则回退到原有的 `sim.ul_data_hex`
- simulator 在收到合法 IPv4 上行 `UL DATA` 后，会通过持久化 N3 socket 把它封成 GTP-U 发给 UPF
- simulator 在轮询到下行 GTP-U 后，会解封 inner IPv4 载荷并排成新的 `DL_OBJ_DATA`

这意味着当前最小用户面闭环已经变成：

- `downlink GTP-U -> gNB -> DL DATA -> UE echo reply -> UL DATA -> gNB -> uplink GTP-U`

### 1.8 可选 TUN UE 用户面

当前已经完成 Stage E1 的可选 TUN 接入：

- `mini_ue_c` 在 `sim.ue_tun_enabled=true` 时会优先启用 `ue_tun`
- gNB 会在 session setup 成功后，把解析得到的 `ue_ipv4` 放进 shared-slot 下行摘要
- UE runtime 看到这个 `ue_ipv4` 后，会配置自己的 TUN 设备
- 后续下行 `DL DATA` 会优先写入 TUN
- UE 会从 TUN 里读出内核返回的 IPv4 包，并在下一次上行 payload grant 上发回 gNB
- 如果没有启用 TUN，或者还没有拿到 `ue_ipv4`，则继续回退到 `ue_ip_stack_min`

默认配置下：

- `sim.ue_tun_isolate_netns=true`
- UE 会在隔离的 user+network namespace 中创建 `miniue0`
- 这样不会污染宿主机默认网络命名空间
- 如果再配置 `sim.ue_tun_netns_name`，UE 会把这个隔离 namespace 发布到 `/var/run/netns/<name>`
- 如果再配置 `sim.ue_tun_add_default_route=true`，UE 会在拿到核心网分配 IP 后安装 `default dev <ue_tun_name>`
- 如果再配置 `sim.ue_tun_dns_server_ipv4`，UE 会写 `/etc/netns/<name>/resolv.conf`

这意味着当前不仅可以做“服务器侧 ping UE IP”，也可以做“UE 侧主动发公网流量”的手工验证：

- `ip netns exec <ue_netns_name> ping -c 4 8.8.8.8`
- `ip netns exec <ue_netns_name> ping -c 4 www.baidu.com`

### 1.9 当前可执行的端到端 ping 路径

当前已经具备手工端到端验证所需的代码路径：

- `server/host -> UPF -> gNB -> UE TUN -> UE kernel stack -> gNB -> UPF -> server/host`

但注意这里的“已具备”指的是：

- gNB/UE 代码路径已经实现
- 仓库里已经提供示例 YAML，UE 也会自动跟随后续 `DL_NAS` 生成 happy-path `UL_NAS`
- 端到端 `ping` 仍然属于手工验证，不属于当前自动化 `ctest`

原因是：

- `mini_ue_c` 只有最小 happy-path UE NAS 逻辑，不是通用完整 NAS 栈
- TUN 依赖宿主机 `/dev/net/tun`、`ip` 命令和真实 Open5GS 环境

### 1.10 已有的 Open5GS 外部验证工具

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

## 2. 当前限制

当前 staged A-E 功能已经全部落地，但仍有这些实际限制：

- `mini_ue_c` 只实现了面向 Open5GS happy-path 的最小 UE NAS 响应链路，不是通用完整 NAS 状态机
- 当前的真实 `ping` 路径仍依赖手工 Open5GS 环境验证，而不是自动化测试
- TUN 端到端演示依赖宿主机权限、`/dev/net/tun`、`ip` 命令和真实的 AMF/UPF
- 如果要从 UE 侧访问公网，Open5GS 宿主机还必须为 UE 子网开启 IPv4 转发和 NAT 或等价路由

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

- `test_config_loads`
- `test_nas_5gs_min_builds_followup_uplinks`
- `test_nas_5gs_min_polls_downlink_exchange`
- `test_shared_slot_link_round_trip`
- `test_shared_slot_link_handles_slot_zero_and_shutdown_boundaries`
- `test_integration_shared_slot_ue_runtime`
- `test_integration_shared_slot_ue_runtime_auto_nas_session_setup`
- `test_integration_core_bridge_prepares_initial_message`
- `test_integration_core_bridge_relays_followup_ul_nas`
- `test_integration_core_bridge_extracts_session_setup_state`
- `test_integration_core_bridge_relays_post_session_nas`
- `test_gnb_core_bridge_prepares_initial_ue_message`
- `test_gnb_core_bridge_relays_followup_uplink_nas`
- `test_gnb_core_bridge_parses_session_setup_state`
- `test_gnb_core_bridge_skips_stale_and_waits_for_future_uplink_nas`
- `test_gnb_core_bridge_relays_post_session_downlink_nas`
- `test_n3_user_plane_activates_and_sends_uplink_gpdu`
- `test_n3_user_plane_polls_downlink_packet`
- `test_ue_ip_stack_min_generates_echo_reply`
- `test_ue_ip_stack_min_ignores_non_ipv4_payload`
- `test_integration_shared_slot_ue_runtime_generates_icmp_reply_payload`
- `test_integration_core_bridge_forwards_ul_ipv4_to_n3`

其中新增的 Stage E 相关自动覆盖主要是：

- `test_config_loads`
  - 校验 `slot_sleep_ms` 和 `ue_tun_*` 默认配置
- `test_open5gs_end_to_end_ue_config_loads_tun_internet_settings`
  - 校验 end-to-end UE 示例里的 `ue_tun_netns_name / ue_tun_add_default_route / ue_tun_dns_server_ipv4`
- `test_shared_slot_link_round_trip`
  - 校验 shared-slot 下行摘要中的 `ue_ipv4` 字段可以稳定往返

当前没有放进 `ctest` 的内容是：

- 真实 TUN 建立和 namespace 交互
- 真实 Open5GS 端到端 `ping`

这些仍然保留为手工验证，因为它们依赖运行环境。

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

注意：当前这一步主要验证第一跳控制面闭环；完整 happy-path attach 会在后面的自动 NAS 测试和 Open5GS 手工验证里覆盖。

## 7. 测试 follow-up UL_NAS / DL_NAS 桥接

### 7.1 测试目标

验证两件事：

- gNB bridge 会继续轮询 `ue_to_gnb_nas/` 并将后续 UE NAS 转发给 AMF
- live `mini_ue_c` 会根据 `gnb_to_ue/*.json` 中的 `DL_NAS` 自动生成后续 `UL_NAS`

### 7.2 自动回归方式

这一阶段最稳的回归方式是直接执行已有测试：

```bash
cd /home/hzy/codex/test2/codex/gnb_c
./build/mini_gnb_c_tests
```

重点看：

- `test_nas_5gs_min_builds_followup_uplinks`
- `test_nas_5gs_min_polls_downlink_exchange`
- `test_integration_shared_slot_ue_runtime_auto_nas_session_setup`
- `test_integration_core_bridge_relays_followup_ul_nas`
- `test_integration_core_bridge_relays_post_session_nas`

### 7.3 可选的手工事件格式

如果你想单独调 bridge，也仍然可以手工写 `UL_NAS` 事件文件。

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

## 9. 测试最小 IPv4/ICMP 用户面

### 9.1 测试目标

验证当前 Stage D1 + D2 已经具备：

- 基于 session state 激活长期存在的 N3 socket
- 下行 GTP-U 解包为 `DL DATA`
- UE 侧最小 `ICMP Echo Request -> Echo Reply`
- 上行 IPv4 重新封成 GTP-U 发回 fake UPF

### 9.2 推荐测试命令

当前最稳的验证方式仍然是测试二进制：

```bash
cd /home/hzy/codex/test2/codex/gnb_c
./build/mini_gnb_c_tests
```

重点看：

- `test_n3_user_plane_activates_and_sends_uplink_gpdu`
- `test_n3_user_plane_polls_downlink_packet`
- `test_ue_ip_stack_min_generates_echo_reply`
- `test_integration_shared_slot_ue_runtime_generates_icmp_reply_payload`
- `test_integration_core_bridge_forwards_ul_ipv4_to_n3`
- `test_integration_core_bridge_extracts_session_setup_state`

### 9.3 预期结果

这些测试通过后，说明：

- N3 helper 已能根据 `upf_ip / upf_teid / qfi / ue_ipv4` 激活
- helper 已能向 loopback UDP peer 发出正确的 GTP-U G-PDU
- simulator 在 session setup 完成后会自动激活一次 N3 helper
- 当前 integration 覆盖中，`simulator.n3_user_plane.activation_count == 1`
- UE 侧已经能把一个下行 ICMP request 变成下一次上行 data grant 上的 ICMP reply
- simulator 已经能把一个合法 IPv4 上行 payload 再封回 GTP-U 发给 fake UPF
- simulator 已会生成可供 Wireshark 检查的 runtime `NGAP` / `GTP-U` pcap

但这还不代表：

- UE 已经有完整 IP 栈
- UE 已经通过真实 NAS 流程学到自己的 session/IP 上下文
- 仅靠这些自动化测试就能断言真实 Open5GS/TUN 环境中的 `ping UE IP` 已通过

## 10. 测试 TUN + Open5GS 端到端 ping

### 10.1 测试目标

验证当前 Stage E1 + E2 已经具备：

- gNB 在 session setup 后将 `ue_ipv4` 送入 shared-slot
- UE 在拿到 `ue_ipv4` 后配置 TUN
- 下行 GTP-U inner IPv4 被注入 UE TUN
- UE 内核协议栈返回 `ICMP Echo Reply`
- UE 从 TUN 读回上行 IPv4 负载并经 gNB/N3 发回 UPF

### 10.2 环境前提

需要：

- 已运行的 Open5GS AMF/UPF
- 可用的 `ogstun`
- 宿主机存在 `/dev/net/tun`
- 宿主机存在 `ip` 命令

当前仓库提供的示例默认按这些地址准备：

- `AMF=127.0.0.5:38412`
- `UPF UDP port=2152`

### 10.3 准备运行目录

现在 live `mini_ue_c` 会自己根据 `DL_NAS` 生成后续 `UL_NAS`，不再需要预置 seed 文件：

```bash
cd /home/hzy/codex/test2/codex/gnb_c
rm -rf out/local_exchange out/shared_slot_link.bin out/summary.json
mkdir -p out/local_exchange
```

### 10.4 启动命令

```bash
cd /home/hzy/codex/test2/codex/gnb_c
./build/mini_ue_c config/example_open5gs_end_to_end_ue.yml &
UE_PID=$!
./build/mini_gnb_c_sim config/example_open5gs_end_to_end_gnb.yml &
GNB_PID=$!
```

说明：

- gNB 示例配置开启了 `core.enabled=true`
- gNB 示例配置开启了 `slot_sleep_ms=10`，便于你在运行中观察和注入流量
- UE 示例配置开启了 `ue_tun_enabled=true`
- UE 示例配置默认开启 `ue_tun_isolate_netns=true`

### 10.5 运行中检查

先看 session 和 UE IP 是否建立：

```bash
sed -n '1,240p' out/summary.json
```

关注这些字段：

- `core_session.amf_ue_ngap_id`
- `core_session.ue_ipv4`
- `core_session.upf_ip`
- `core_session.upf_teid`
- `core_session.qfi`
- `ngap_trace_pcap_path`
- `gtpu_trace_pcap_path`

还可以直接检查 runtime pcap 是否已经生成：

```bash
ls -l out/gnb_core_ngap_runtime.pcap out/gnb_core_gtpu_runtime.pcap
```

如果 UE TUN 路径已经接上，UE 日志里通常会出现类似：

- `UE configured TUN ...`
- `UE injected DL DATA into TUN ...`
- `UE captured TUN uplink packet ...`

同时建议检查 runtime pcap：

```bash
ls -l out/gnb_core_ngap_runtime.pcap out/gnb_core_gtpu_runtime.pcap
```

### 10.6 宿主机或服务器侧 ping UE

拿到 `summary.json` 里的 `ue_ipv4` 之后，从宿主机或服务器侧发包：

```bash
ping -c 4 <ue_ipv4_from_summary>
```

在默认 Open5GS 数据面下，开发时常见的 UE IP 是：

- `10.45.0.7`

但实际测试时仍应以 `summary.json` 为准。

### 10.7 UE 侧主动 ping 公网地址

先确认 Open5GS 宿主机已经允许 UE 子网出公网。最小前提通常是：

```bash
sysctl -w net.ipv4.ip_forward=1
iptables -t nat -A POSTROUTING -s 10.45.0.0/16 -o <host_uplink_if> -j MASQUERADE
```

然后从 UE 的隔离 namespace 里发流量：

```bash
ip netns exec miniue-demo ping -c 4 8.8.8.8
ip netns exec miniue-demo ping -c 4 www.baidu.com
```

建议按这个顺序排查：

- 先测 `8.8.8.8`
- 再测 `www.baidu.com`

如果第一个失败，问题通常在：

- UE 默认路由没有装上
- Open5GS/UPF 出公网没有 NAT 或转发
- N3 用户面链路没通

如果第一个成功、第二个失败，问题通常在：

- `sim.ue_tun_dns_server_ipv4` 没配置
- `/etc/netns/miniue-demo/resolv.conf` 不存在
- DNS 服务器本身不可达

### 10.8 辅助观测命令

```bash
ip -s link show dev ogstun
tcpdump -ni ogstun icmp
ip netns exec miniue-demo ip addr
ip netns exec miniue-demo ip route
cat /etc/netns/miniue-demo/resolv.conf
```

期望现象：

- `ogstun` 收发计数增加
- `tcpdump` 能看到 `Echo Request / Echo Reply`
- `summary.json` 中保留有效的 session state

### 10.8 结束进程

```bash
wait $GNB_PID
wait $UE_PID
```

## 11. 当前测试结论应该怎么理解

如果以下三类测试都通过：

- `ctest --test-dir build --output-on-failure`
- 本地 `mini_ue_c -> mini_gnb_c_sim` 闭环
- `ngap_probe --replay`
- 手工 `TUN + Open5GS` 端到端验证

那么当前可以认为：

- 本地共享寄存器式 UE 与 gNB 联动已经成立
- gNB 到 AMF 的最小控制面桥接已经成立
- simulator 侧已经能提取会话建立得到的关键 session state
- 持久化 N3 socket 基础层已经成立
- UE 侧可选 TUN 路径已经成立
- `server -> UPF -> gNB -> UE -> gNB -> UPF` 的手工 `ping` 路径已经具备

但还不能认为：

- `mini_ue_c` 已具备完整 UE NAS 行为
- 当前仓库已经有完整自动化 UE attach/NAS 流程
- 当前仓库已经有自动化 CI 级别的真实 TUN/Open5GS `ping` 回归
