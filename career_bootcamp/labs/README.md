# EdgeBot C++ 系统练习

本目录是既有 `/home/ring/C-20_Learn/labs` 的补充，重点覆盖事件循环、设备协议、重连、指标和状态机。

starter 中的函数目前会抛出“未实现”异常。测试会先失败，这是预期行为。请直接修改 `src/labs.cpp`，必要时可以调整 `labs.hpp` 中的私有成员，但应保持公开接口和测试定义的语义。

## 构建

```bash
cd /home/ring/C-20_Learn/career_bootcamp/labs
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/edgebot_lab_tests
```

使用 CTest：

```bash
ctest --test-dir build --output-on-failure
```

启用 ASan/UBSan 的示例：

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## Lab 01：有界优先级事件循环

实现：

- `EventLoop::post`
- `EventLoop::post_high_priority`
- `EventLoop::run_once`
- `EventLoop::request_stop`

必须满足：

1. 高优先级任务先于普通任务。
2. 同优先级任务按投递顺序执行。
3. `run_once` 执行本次调用开始时已经就绪的任务。
4. 回调中投递的新任务留到下一次 `run_once`。
5. 队列满返回 `full`，不阻塞、不偷偷扩容。
6. 停止后拒绝新任务，但停止前已接受的任务仍可通过 `run_once` 消费。
7. 任意任务抛异常时，记录错误并继续执行本批次其他任务。

思考题：

- 为什么这里要有固定批次？
- 如果任务回调永远投递新的高优先级任务，会出现什么？
- 多个外部线程投递时，如何保证不直接修改业务状态？

## Lab 02：定时器

实现 `EventLoop::post_after` 和 `run_once` 中的到期定时器处理。

必须满足：

1. 按截止时间顺序执行。
2. 相同截止时间按投递顺序执行。
3. 未到期定时器不能执行。
4. 定时器转成就绪任务后，在本次循环中执行。
5. 定时器数量也受容量限制。
6. 停止后拒绝新定时器。

测试使用可控制的 `steady_clock::time_point`，不要使用真实等待。

思考题：

- 定时器到期后是直接执行，还是进入同一任务队列？
- 时钟向前跳变时如何避免一次执行过多任务？
- 取消定时器后如何防止悬空回调和误触发？

## Lab 03：增量帧编码与解析

帧格式：

```text
offset  size  field
0       2     magic = AA 55
2       1     version = 01
3       1     type
4       2     sequence, little-endian
6       2     payload_length, little-endian
8       N     payload
8+N     2     CRC16/CCITT-FALSE, little-endian
```

CRC 计算范围为 `version` 到 payload 最后一个字节。

实现：

- `crc16_ccitt`
- `encode_frame`
- `FrameDecoder::feed`
- `FrameDecoder::next`

必须满足：

- 任意切分位置都能解析。
- 一次输入可以包含多个帧。
- magic 前的垃圾字节应被安全跳过。
- payload 长度超过上限时返回错误，不允许超大分配。
- 未知版本返回 `unsupported_version`。
- CRC 失败返回 `checksum`，之后仍能重新同步到后续正常帧。
- `std::nullopt` 只表示需要更多字节。

思考题：

- 为什么不能直接把结构体内存写入线路？
- 长度字段为什么必须检查加法和上限？
- 校验失败后为什么不能直接清空整个缓冲区？
- 如何为解析器编写模糊测试？

## Lab 04：指数退避

实现 `ReconnectPolicy::next_delay`。

规则：

1. 第一次返回初始间隔。
2. 后续每次翻倍，但不超过最大间隔。
3. 次数持续增加，即使延迟已达到最大值。
4. `reset` 后重新从初始间隔开始。
5. 初始化参数非法时抛出 `std::invalid_argument`。

测试参数为 100 ms 到 1000 ms。

思考题：

- 生产中为什么通常要加入随机抖动？
- 连接稳定多久后应重置退避？
- 设备永久离线时是否应无限重连？

## Lab 05：有界延迟统计

实现 `LatencyRecorder`。

要求：

- 最多保留 `maximum_samples` 个样本。
- 超过容量时覆盖最旧样本。
- `add` 拒绝负延迟。
- `percentile(p)` 只接受 `0 <= p <= 1`。
- 空样本返回 `std::nullopt`。
- 采用 nearest-rank：对 n 个升序样本，索引为 `ceil(p * n) - 1`。
- `0` 分位数返回最小值，`1` 返回最大值。

思考题：

- 为什么系统关注 p99，而不只看平均值？
- 环形覆盖样本对长期统计有什么偏差？
- 如果采样频率不均匀，应如何记录权重？

## Lab 06：设备状态机

实现 `DeviceStateMachine::handle`。

状态迁移：

| 当前状态 | 事件 | 新状态 |
|---|---|---|
| idle | connect_requested | connecting |
| connecting | connection_opened | online |
| online | heartbeat_lost | degraded |
| degraded | heartbeat_restored | online |
| 任意未关闭状态 | close_requested | closed |

要求：

- 非法事件返回 `false`，状态不变。
- 已关闭状态不接受任何事件。
- 状态机不负责打印日志、sleep 或实际重连。

思考题：

- 为什么状态机适合与 IO 适配器分离？
- 断线时如何清理旧定时器和旧命令？
- 进入 degraded 后是否仍允许控制命令？谁做决定？

## 提交标准

- 所有测试通过。
- 高警告级别构建无可归因于新增代码的警告。
- ASan/UBSan 构建通过。
- 不通过修改测试掩盖错误。
- 能解释每个容器选择的复杂度和生命周期。
- 能画出事件循环一轮、帧解析状态和设备状态机。

