# 虚拟磁盘阵列项目

实现一个 raid4 阵列，校验计算设备为模拟的 pcie 设备。

## 项目架构

![](image.png)

项目本身目的是做一个使用外围的 FPGA 或者 GPU 设备进行校验计算的 RAID 阵列，内核创建块设备劫持文件系统发来的 bio 交由驱动程序处理，数据由主机或者外围设备发送给 SSD。本仓库是该项目的纯软件模拟，目的是设计和验证交互协议。虚拟的 pcie 设备由 NVMeVirt[1] 项目的部分代码修改实现。

## 组成

* device：pcie 虚拟设备，管理从物理内存中取得的存储资源，其中前`STORAGE_OFFSET`字节的区域是设备的 bar 空间，偏移0处是`struct pciev_bar`，其之后为 chunk bitmap；bar 空间之后的区域是一个数组，存储了每个 chunk 的上次更新时间，标识每个 chunk 的冷热属性。bitmap 的每个 bit 表示一个 chunk 被更新了，由主机在更新的时候设置这个 bit 为 1，设备在读取到这个 bit 时将这个 bit 设为 0 并且重写这个 chunk 的更新时间。

* block：面向文件系统的块设备，不使用 muti-queue 机制，直接注册`.submit_bio`接口作为`struct bio`的处理函数，上层调用`submit_bio`函数后会直接调用这个接口不会进入队列机制，省去了一次经过 IO schedule 层的过程。将`struct bio`按照 chunk 使用`bio_split`拆分为若干面向单个 nvme 设备的小`struct bio`。如果当前操作为‘写’，则提交小的 bio 之前要修改对应位置的 bitmap。

* pciedrv：pcie 驱动，校验操作的主要执行模块。轮询时遍历 bitmap 进行置位和重写更新时间，之后遍历每个 stripe，如果当前 stripe 中有至少一个 chunk 是脏的冷 chunk ，就把这个 stripe 计算校验写给校验盘。

## 测试

在测试之前，在`/etc/default/grub`中添加一行`GRUB_CMDLINE_LINUX="memmap=1G\\\$5G"`，使物理
内存中从5GB开始，1GB的空间不被映射。重启之后使用`sudo cat /proc/iomem`确认是否保留相应地>址。

使用`setup.sh`安装模块，该脚本中写入了和上述参数对应的模块参数

子目录中 testbio 是测试提交 bio 的模块；readtest 目录是直接使用 bio 读取 几个 ssd 设备的头部数据到 dmesg 里面的模块，使用 read.sh 脚本读取和 clearhead.sh 清除头部数据，方便调试。如果使用 dd 命令读取的话，会遇到更新不及时的问题，可能是快设备的缓存导致的。

## 展望

目前的模块 demo 实现了 praid 校验的基本功能，未来可以开发以下功能：

1. IO 或者校验的启停机制和单盘的恢复功能：通过设置 bar 寄存器的值，控制设备 dispatch 的启动或者停止，此时所有 IO 请求暂时被阻塞或者拒绝，用户可以取出至多一个硬盘，插入新的硬盘之后进行该硬盘数据的恢复

2. 用户态控制应用：通过 proc 或者其他方式控制或者读取模块的状态

3. 优化校验计算方式：本模块的计算方式本质上是一个延迟的校验计算，采用的计算方式每次需要读入该 stripe 所有 chunk 的数据；事实上，对于 ssd 较多的情况，只需获取 write 操作对应的 chunk 在写入前的数据、write 的数据和校验盘的原数据就可以计算校验盘的新数据，而不是读取所有盘的数据进行计算。但是在本模块的架构中，由于 write 操作和校验计算操作没有一对一关系，设备的校验端无法保证在 ssd 进行 write 之前读取原来的数据，故这种计算方案不合适。为了提高某些情况下的效率（尤其是在 ssd 数量不可忽略的情况下），首先优化对 chunk 冷热性质的判别吗，然后将数据冷热关系对主机进行同步，对于一些冷 chunk 的少量 write 操作，先读取他的原数据在进行write，并且主机将原数据和新数据直接传输给设备端（或者设备直接通过 p2p 读取原数据），就可以只使用三个 chunk 就计算出校验。

4. 设备的 dispatcher 多线程化：可能要关注内存模型是否线程安全

## reference

[1] [NVMeVirt](https://github.com/snu-csl/nvmevirt)