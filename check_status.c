#include <stdio.h>

#define CMDLS_ON	1
#define CMDCP_ON	1
#define CMDMV_ON	1
#define CMDMD_ON	1
#define CMDRM_ON	1
#define CMDCP2L_ON	1
#define CMDCP2FS_ON	1
#define CMDCD_ON	1
#define CMDPWD_ON	1
#define CMDTOUCH_ON	1
#define CMDCAT_ON	1

int main() {
    printf("|---------------------------------|\n");
    printf("|------- Command ------|- Status -|\n");
#if (CMDLS_ON == 1)
    printf("| ls                   |    ON    |\n");
#else
    printf("| ls                   |    OFF   |\n");  
#endif
#if (CMDCD_ON == 1)
    printf("| cd                   |    ON    |\n");  
#else
    printf("| cd                   |    OFF   |\n");  
#endif
#if (CMDMD_ON == 1)
    printf("| md                   |    ON    |\n");  
#else
    printf("| md                   |    OFF   |\n");  
#endif
#if (CMDPWD_ON == 1)
    printf("| pwd                  |    ON    |\n");  
#else
    printf("| pwd                  |    OFF   |\n");  
#endif
#if (CMDTOUCH_ON == 1)
    printf("| touch                |    ON    |\n");  
#else
    printf("| touch                |    OFF   |\n");  
#endif
#if (CMDCAT_ON == 1)
    printf("| cat                  |    ON    |\n");  
#else
    printf("| cat                  |    OFF   |\n");  
#endif
#if (CMDRM_ON == 1)
    printf("| rm                   |    ON    |\n");  
#else
    printf("| rm                   |    OFF   |\n");  
#endif
#if (CMDCP_ON == 1)
    printf("| cp                   |    ON    |\n");  
#else
    printf("| cp                   |    OFF   |\n");  
#endif
#if (CMDMV_ON == 1)
    printf("| mv                   |    ON    |\n");  
#else
    printf("| mv                   |    OFF   |\n");  
#endif
#if (CMDCP2FS_ON == 1)
    printf("| cp2fs                |    ON    |\n");  
#else
    printf("| cp2fs                |    OFF   |\n");  
#endif
#if (CMDCP2L_ON == 1)
    printf("| cp2l                 |    ON    |\n");  
#else
    printf("| cp2l                 |    OFF   |\n");
#endif
    printf("|---------------------------------|\n");
    return 0;
}

