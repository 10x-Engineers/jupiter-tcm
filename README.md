# 下载

在开发板上编译

```
git clone https://gitee.com/bianbu-linux/tcm.git -b bl-v2.0.y
cd tcm
make
./app.elf
```



# 说明

- tcm 是一块高速sram，K1/M1上共512K。可以给AI CPU高速缓存数据，而不用每次去dram取数据。

- udma 实现用户层控制dma做tcm和dram之间数据搬运。也可以直接用CPU搬运而不用dma。

- 如果有AI推理，onnxruntime也会用到tcm。其他应用直接访问tcm会产生资源竞争，导致推理速度下降。

使用方法参考main.c文件，实现了数据在tcm和dram之间互相拷贝。访问tcm前先确保自身进程绑定在AI CPU上，也就是CPU 0-3.

