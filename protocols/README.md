# App 与设备通信协议

[Corallium v1](corallium-v1/README.md) 定义 Corallium 与设备的消息、数据结构和信道。
目前实现对象是 M5Stack StopWatch 陪伴时钟与 ESP-Mosaico 衍生固件。
已有 Core2 固件没有此服务，不因同属设备仓库而被当作兼容设备。

协议采用能力声明。App 发现指定 BLE service 后读取 `device.info`，仅为设备
声明的能力提供可执行操作。设备照片和型号名称用于展示，不能证明协议兼容。

在仓库根运行以下命令检查合同与示例：

```sh
python3 -m venv tmp/protocol-test-env
tmp/protocol-test-env/bin/pip install -r protocols/tests/requirements.txt
tmp/protocol-test-env/bin/python protocols/tests/check_contract.py
```

App 的
codec/模拟交互测试和各设备构建命令见各自 README。
