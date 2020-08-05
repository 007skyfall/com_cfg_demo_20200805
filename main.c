#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <wchar.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#include "iniparser.h"
#include "dictionary.h"
#include "rw_ini_file_func.h"
#include "serial.h"

//#define COM_CONF_FILE_PATH               "/irdna/config/com_conf.ini"
#define COM_NUM                          "/dev/ttyAMA2"
#define CFG_FILE_NAME                    "com_conf.ini"
#define MAX_VALUE_BUFF                   (64)
#define DEFAULT_BAUD                     (115200)
#define DEFAULT_WRITE_LEN                (64)
#define MAX_WRITE_VALUE_LEN              (256)
#define MAX_PATH_LEN                     (256)

//#define COM_DEBUG
#ifdef  COM_DEBUG
#define com_pr(...)	do{ printf("\n\r [font] " __VA_ARGS__); }while(0)
#else
#define com_pr(...)
#endif

char g_app_absolute_path[MAX_PATH_LEN]      = { 0 };
dictionary *g_pcom_dict                     = NULL;
char g_com_dev_node[MAX_VALUE_BUFF]         = { 0 };                 //串口设备节点
unsigned int g_com_baud                     = 0;                     //波特率
unsigned int g_wr_len                       = 0;
unsigned int g_rd_len                       = 0;
char g_wr_value_array[MAX_WRITE_VALUE_LEN]  = { 0 };


int get_app_abs_path(void)
{
    char current_absolute_path[MAX_PATH_LEN] = { 0 };
    int ret                         = 0;
    int count                       = 0;
    int i                           = 0;
    //获取当前程序绝对路径
    memset(current_absolute_path, 0, sizeof(current_absolute_path));
    
    count = readlink("/proc/self/exe", current_absolute_path, MAX_PATH_LEN);
    if (count < 0 || count >= 128)
    {
        printf("readlink err \n");
        ret = -1;
        return ret;
    }
    
    //获取当前目录绝对路径，即去掉程序名
    for (i = count; i >= 0; --i)
    {
        if (current_absolute_path[i] == '/')
        {
            current_absolute_path[i+1] = '\0';
            break;
        }
    }
    
    strcpy(g_app_absolute_path, current_absolute_path);

    return 0;
}

int init_com_cfg(char * path)
{
    int ret = 0;
    int i   = 0;

    char value_buff[MAX_VALUE_BUFF] = {0};
    memset(value_buff, 0, sizeof(value_buff));
    
    sprintf(path, "%s%s", path, CFG_FILE_NAME);
    printf("path = %s\n", path);
    g_pcom_dict = OpenINI(path);
    if(NULL == g_pcom_dict)
    {
        printf("load ini file failed!\n");
        ret = -1;
        return ret;
    }
    
    memset(g_com_dev_node, 0 ,sizeof(g_com_dev_node));
    GetPrivateProfileString("com_cfg", "com", g_com_dev_node, COM_NUM, g_pcom_dict);
    com_pr("g_com_dev_node = %s\n",g_com_dev_node);   

    g_com_baud = GetPrivateProfileInt("com_cfg", "baud", DEFAULT_BAUD, g_pcom_dict);
    com_pr("g_com_baud = %d\n",g_com_baud);  
    
    memset(g_wr_value_array, 0, sizeof(g_wr_value_array));
    g_wr_len = GetPrivateProfileInt("write_cfg", "w_len", DEFAULT_WRITE_LEN, g_pcom_dict);
    
    com_pr("g_wr_len = %d\n",g_wr_len);  
    for(i=0; i<g_wr_len; ++i)
    {
        sprintf(value_buff, "value_%d", i+1);
        GetPrivateProfileString("write_cfg", value_buff, &g_wr_value_array[i], &g_wr_value_array[i], g_pcom_dict);
        g_wr_value_array[i]  = strtol(&g_wr_value_array[i], NULL, 16);
        
    }
    for(i=0; i<g_wr_len; ++i)
    {
        com_pr("g_wr_value_array[%d] = %02x  ", i, g_wr_value_array[i]);  
    }

    printf("\n");

    g_rd_len = GetPrivateProfileInt("read_cfg", "r_len", 0, g_pcom_dict);
    com_pr("g_rd_len = %d\n",g_rd_len);  

    CloseINI(g_pcom_dict);
    
    return ret;
}

int main(int argc, const char *argv[])
{
    int ret             = 0;
    int write_len       = 0;
    int pelco_com_fd    = 0;
    int nDelays         = 10 * 1000;
    int port_status     = 0;
    int read_len        = 0;
	char rcv_buf[32]    = { 0 };
	int pre_rcv_len     = 60;
    int i               = 0;

//(0) init com configure file
    ret = get_app_abs_path();
    if (0 != ret)
    {
        printf("get_app_abs_path err!\n");
        return ret;
    }

    init_com_cfg(g_app_absolute_path);
    if (0 != ret)
    {
        printf("init_com_cfg err!\n");
        ret = -1;
        return ret;
    }
    
//(1)  open pelco_com
    
    pelco_com_fd = open_port(g_com_dev_node);
    
    if (0 > pelco_com_fd)
    {
        printf("Open Serial Port err!\n");
        ret = -2;
        return ret;
    }
    
    usleep(nDelays);
        
//(2) set pelco_com baud
    
    port_status = set_opt(pelco_com_fd, g_com_baud, 8, 'N', 1);

    if (port_status < 0)
    {
        printf("set_opt error\n");
        ret = -3;
        return ret;
    }
    
//(3) begin to send data 

     write_len = write(pelco_com_fd, g_wr_value_array, g_wr_len);
    
     if(write_len < 0)
      {
        perror("write to port");
        ret = -4;
        return ret;
      }
         
    usleep(1000);

    printf("write_len=%d\n", write_len);
    printf("write to port is ok!\n");
    
//(4) begin to receive data 
    pre_rcv_len = g_rd_len;
    memset(rcv_buf, 0, sizeof(rcv_buf));
    read_len = read_port(pelco_com_fd,rcv_buf, pre_rcv_len, 1000);
    printf("read_len = %d\n",read_len);

    printf("response  data:\n");
        for(i=0; i<pre_rcv_len; ++i)
        {
            printf("%02x ",rcv_buf[i] );
        }
        puts("");
        
//(5) stop 
    close(pelco_com_fd);

	return 0;	
}

