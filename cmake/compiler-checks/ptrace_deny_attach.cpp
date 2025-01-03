#include <sys/ptrace.h>
#include <sys/types.h>

int main()
{
    ptrace(PT_DENY_ATTACH, 0, 0, 0);
    return 0;
}
