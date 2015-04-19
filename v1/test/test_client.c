#include "fakeudp.h"

int main(int argc, char *argv[])
{
	if (argc != 6) {
		printf("Usage: %s msg src_ip src_port dst_ip dst_port\n", argv[0]);
		return -1;
	}
	int ret = SendFakeUdp(argv[1], strlen(argv[1]), argv[2], strtoul(argv[3], NULL, 0), argv[4], strtoul(argv[5], NULL, 0));
	if (ret != 0) {
		printf("SendFakeUdp fail. ret=%d\n", ret);
		return -1;
	}
	printf("SendFakeUdp succ.\n");
	return 0;
}
