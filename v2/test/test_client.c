#include "fakeudp.h"

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Usage: %s src_ip src_port dst_ip dst_port\n", argv[0]);
        return -1;
    }

    //1. 构造FakeUDP对象，最后一个参数指定用户缓冲区的最大长度
    FakeUDP obj(argv[1], strtoul(argv[2], NULL, 0), argv[3], strtoul(argv[4], NULL, 0), 1024);
    if(obj.Init() == false)
    {
        return -1;
    }

    //2.构造需要发送的数据
    //将数据填充到用户缓冲区m_pUserData字段，该缓冲区最大长度为m_nMaxUserDataLen
    const char* str = "testing.....ahhahahha";
    sprintf(obj.m_pUserData,  "%s\n",  str);

    //3.发送数据
    int ret = obj.Send(strlen(str));
    if (ret != 0)
    {
        printf("SendFakeUdp fail. ret=%d\n", ret);
        return -1;
    }
    printf("SendFakeUdp succ.\n");
    return 0;
}
