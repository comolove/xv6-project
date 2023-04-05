#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char* argv[]) {

	__asm__("int $128");
	return 0;

	if(argc <= 1) {
		exit();
	}

	if(strcmp(argv[1],"\"user\"")!=0) {
		exit();
	}
	char *buf = "Hello xv6!";
	int ret_val;
	ret_val = myFunction(buf);
	printf(1, "Return value : 0x%x\n", ret_val);
	exit();
}
