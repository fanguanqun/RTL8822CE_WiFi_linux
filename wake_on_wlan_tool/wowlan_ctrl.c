#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>              /* gethostbyname, getnetbyname */
#include <unistd.h>
#include <sys/socket.h>         /* for "struct sockaddr" et al  */
#include <sys/time.h>           /* struct timeval */

#include <linux/version.h>

//#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
//#include <net/if.h>
//#else
unsigned int if_nametoindex(const char *ifname);
//#endif

#include <net/ethernet.h>       /* struct ether_addr */
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/wireless.h>
#include <pthread.h>

#define BUF_SIZE             1024
char interface[IFNAMSIZ];

struct wowlan_crtl_param{
unsigned int subcode;
unsigned int subcode_value;
unsigned int wakeup_reason;
unsigned int len;
unsigned char pattern[0];
};

#define WOWLAN_PATTERN_MATCH 1
#define WOWLAN_MAGIC_PACKET  2
#define WOWLAN_UNICAST       3
#define WOWLAN_SET_PATTERN   4
#define WOWLAN_DUMP_REG      5
#define WOWLAN_ENABLE        6
#define WOWLAN_DISABLE       7
#define WOWLAN_STATUS		 8
#define WOWLAN_DEBUG_RELOAD_FW   9
#define WOWLAN_DEBUG_1   10
#define WOWLAN_DEBUG_2   11

#define Rx_Pairwisekey			1<<0
#define Rx_GTK					1<<1
#define Rx_DisAssoc				1<<2
#define Rx_DeAuth				1<<3
#define FWDecisionDisconnect	1<<4
#define Rx_MagicPkt				1<<5
#define FinishBtFwPatch			1<<7

int getPrivFuncNum(char *iface, const char *fname,int sk) {
    struct iwreq wrq;
    struct iw_priv_args *priv_ptr;
    int i, ret;
    unsigned char pbuf[80];
    pbuf[79]=0x0;
    strncpy(wrq.ifr_name, iface, sizeof(wrq.ifr_name));
    memcpy(pbuf, fname, strlen(fname));
    pbuf[strlen(fname)]=0x0;
    wrq.u.data.pointer =pbuf;// mBuf;
    wrq.u.data.length =  strlen(fname)+1;// sizeof(mBuf) / sizeof(struct iw_priv_args);
    //printf("\n wrq.u.data.length=%d, fname=%s,%s \n",wrq.u.data.length,pbuf,fname);
	wrq.u.data.flags = 0;
    if ((ret = ioctl(sk, SIOCGIWPRIV, &wrq)) < 0) {
        printf("%s,%s getPrivFuncNum - ioctl fail\n",iface,wrq.ifr_name );
        printf("SIOCGIPRIV failed: %d\n", ret);
        return ret;
    }
    priv_ptr = (struct iw_priv_args *)wrq.u.data.pointer;
    for(i=0;(i < wrq.u.data.length);i++) {
        if (strcmp(priv_ptr[i].name, fname) == 0)
            return priv_ptr[i].cmd;
    }
    return -1;
}


int startWoWlanCtrl(char *iface,int subcode,char *pbuf,int pbuf_len) {
    struct iwreq wrq;
    int fnum, ret;
    int skfd;
    
	skfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (skfd < 0) {
        printf("startWoWlanCtrl  - failed to open socket");
        return -1;
    }
    if (!iface || (iface[0] == '\0')) {
        printf("startWoWlanCtrl  - wrong interface");
       // iface = mIface;
        return -1;
    }
//#if 
//    fnum = getPrivFuncNum(iface, "WowlanCtrl",skfd);
 //   if (fnum < 0) {
//        printf("%s,wowlan - function not supported\n",iface);
//        return -1;
//    }
//#else
	fnum=SIOCIWFIRSTPRIV+0xe;
//#endif
    strncpy(wrq.ifr_name, iface, sizeof(wrq.ifr_name));
    wrq.u.data.length = pbuf_len;//12;//0;
    wrq.u.data.pointer = pbuf;//mBuf;
    wrq.u.data.flags = 0;
    ret = ioctl(skfd, fnum, &wrq);
    //printf("startWoWlanCtrl: %d [0x%x]", ret,ret);
    return ret;
}


char xtod(char c) {
  if (c>='0' && c<='9') return c-'0';
  if (c>='A' && c<='F') return c-'A'+10;
  if (c>='a' && c<='f') return c-'a'+10;
  return c=0;        // not Hex digit
}
int HextoDec(char *hex, int l)
{
    if ((*hex==0)||(*hex==',')) return(l);
    return HextoDec(hex+1, l*16+xtod(*hex)); // hex+1?
}
int xstrtoi(char *hex)      // hex string to integer
{
     return HextoDec(hex,0);
}

int get_pattern(char *file_name,int file_len,char *pbuf,int * buf_len){
	FILE *fp;
	fp=fopen("pattern.txt","rb");
	char temp_buf[BUF_SIZE];
	char temp_buf2[BUF_SIZE];
	char *pch_n, *pch_common, *ptr;
	int idx=0;
	if(fp)
	{
		printf("\n open file successful!\n");
		memset(temp_buf, 0, BUF_SIZE);
		memset(temp_buf2, 0, BUF_SIZE);
		//read pattern from file
		fread(temp_buf,sizeof(char),BUF_SIZE,fp);
		fclose(fp);
		{	
			printf("\n file_len=%d\n",file_len);
			//get token split by "\n"
			pch_n = strtok(temp_buf, "\n");
			//record buffer position
			ptr = temp_buf2;

			//keep on getting token split by "\n"
			while(pch_n != NULL)
			{
				//printf(" %s ", pch_n);
				//printf(" %s(%d) ", pch_n, strlen(pch_n));
				//copy the string which is without "\n"
				memcpy(ptr, pch_n, strlen(pch_n));
				ptr += strlen(pch_n);
				pch_n = strtok(NULL, "\n");
			}
			//printf("\n %s\n", temp_buf2);
			ptr = pbuf;
			//get token split by ","
			pch_n = strtok(temp_buf2, ",");
			while(pch_n != NULL)
			{

				//printf(" %02x ", xstrtoi(pch_n));
				pbuf[idx++]= xstrtoi(pch_n);
				//keep on getting token split by ","		
				pch_n = strtok(NULL, ",");
			}
			*buf_len = idx;
		}
	}
	else{
		printf("\n open file failed!\n");
	}
	printf("\n get_pattern: buf_len=%d\n",*buf_len);
	/*{
		int i;
		for(i=0; i < (*buf_len); i++)
			printf(" 0x%02x ", pbuf[i]);
	}*/
	return 0;
}

//----------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
	int	ret = 0;

	struct wowlan_crtl_param *pwowlan_ctrl_param;
	//printf("argc=%d, argv[1]=%s, argv[2]\n", argc, argv[1],argv[2]);
	//allocate wake on wlan structure size + pattern size
	pwowlan_ctrl_param = malloc(sizeof(struct wowlan_crtl_param)+BUF_SIZE);
	if(!pwowlan_ctrl_param)
		return -1;
	//pwowlan_ctrl_param->pattern = (unsigned char *)pwowlan_ctrl_param+sizeof(struct wowlan_crtl_param);
	//printf("%p, %p, %d \n", pwowlan_ctrl_param, pwowlan_ctrl_param->pattern, sizeof(struct wowlan_crtl_param));
	//default
	strncpy(interface, "wlan0", IFNAMSIZ);	

	if (argc == 4) {
		strncpy(interface, argv[1], IFNAMSIZ);
		//printf("interface=%s, subcode=%s.\n", argv[1],argv[2]);
		pwowlan_ctrl_param->subcode=atoi(argv[2]);
		//printf("wowlan_crtl_param->subcode=%d.\n", pwowlan_ctrl_param->subcode);

		switch(pwowlan_ctrl_param->subcode){
			case WOWLAN_PATTERN_MATCH:
			pwowlan_ctrl_param->len=sizeof(struct wowlan_crtl_param);
			pwowlan_ctrl_param->subcode_value=atoi(argv[3]);
			break;
			
			case WOWLAN_MAGIC_PACKET://turn on/off the magic packet 
			pwowlan_ctrl_param->len=sizeof(struct wowlan_crtl_param);
			pwowlan_ctrl_param->subcode_value=atoi(argv[3]);
			break;
			
			#if 0 //UNICAST not support
			//case WOWLAN_UNICAST:
			//pwowlan_ctrl_param->len=sizeof(struct wowlan_crtl_param);
			//pwowlan_ctrl_param->subcode_value=atoi(argv[3]);
			//break;
			#endif //UNICAST not support
			
			case WOWLAN_SET_PATTERN://setting the pattern match content
			printf("\n call get_pattern, pwowlan_ctrl_param=%p, pwowlan_ctrl_param->pattern=%p\n"
			,pwowlan_ctrl_param, pwowlan_ctrl_param->pattern);
			get_pattern(argv[3],BUF_SIZE,pwowlan_ctrl_param->pattern,&pwowlan_ctrl_param->len); 
			{	
				int i;
				printf("\n call get_patterni: len=%d\n",pwowlan_ctrl_param->len);
				for(i=0;i<pwowlan_ctrl_param->len;i=i+8)
					printf("0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n"
					,pwowlan_ctrl_param->pattern[i],pwowlan_ctrl_param->pattern[1+i]
					,pwowlan_ctrl_param->pattern[2+i],pwowlan_ctrl_param->pattern[3+i]
					,pwowlan_ctrl_param->pattern[4+i],pwowlan_ctrl_param->pattern[5+i]
					,pwowlan_ctrl_param->pattern[6+i],pwowlan_ctrl_param->pattern[7+i]);

			}
			pwowlan_ctrl_param->len+=sizeof(struct wowlan_crtl_param);

			printf("\n get_pattern pwowlan_ctrl_param->len=%d\n",pwowlan_ctrl_param->len);

			break;
			
			case WOWLAN_DUMP_REG:
				pwowlan_ctrl_param->len=sizeof(struct wowlan_crtl_param);
				pwowlan_ctrl_param->subcode_value=atoi(argv[3]);
				break;
			
			case WOWLAN_STATUS:
				pwowlan_ctrl_param->len=sizeof(struct wowlan_crtl_param);
				pwowlan_ctrl_param->subcode_value=0;
				break;
			case WOWLAN_DEBUG_RELOAD_FW:
				pwowlan_ctrl_param->len=sizeof(struct wowlan_crtl_param);
				pwowlan_ctrl_param->subcode_value=atoi(argv[3]);
				break;
			case WOWLAN_DEBUG_1:
				pwowlan_ctrl_param->len=sizeof(struct wowlan_crtl_param);
				pwowlan_ctrl_param->subcode_value=atoi(argv[3]);
				break;
			case WOWLAN_DEBUG_2:
				pwowlan_ctrl_param->len=sizeof(struct wowlan_crtl_param);
				pwowlan_ctrl_param->subcode_value=atoi(argv[3]);
				break;

			default:
				printf("\n No such command!\n");
				goto _exit;
		}

		//printf("\n pwowlan_ctrl_param->len=%d\n",pwowlan_ctrl_param->len);
		startWoWlanCtrl(interface,pwowlan_ctrl_param->subcode, (unsigned char *)pwowlan_ctrl_param,pwowlan_ctrl_param->len);
		if(pwowlan_ctrl_param->subcode == WOWLAN_STATUS)
		{
			//printf("\n pwowlan_ctrl_param->wakeup_reason = 0x%02x\n", pwowlan_ctrl_param->wakeup_reason);
			switch(pwowlan_ctrl_param->wakeup_reason)
			{
				case Rx_Pairwisekey:
					printf("%d Rx_Pairwisekey\n", Rx_Pairwisekey);
					break;
				case Rx_GTK:
					printf("%d Rx_GTK\n", Rx_GTK);
					break;
				case Rx_DisAssoc:
					printf("%d Rx_DisAssoc\n", Rx_DisAssoc);
					break;
				case Rx_DeAuth:
					printf("%d Rx_DeAuth\n", Rx_DeAuth);
					break;
				case FWDecisionDisconnect:
					printf("%d FWDecisionDisconnect\n", FWDecisionDisconnect);
					break;
				case Rx_MagicPkt:
					printf("%d Rx_MagicPkt\n", Rx_MagicPkt);
					break;
				case FinishBtFwPatch:
					printf("%d FinishBtFwPatch\n", FinishBtFwPatch);
					break;
				default:
					printf("0 UNKNOW reason, reference: %d\n", pwowlan_ctrl_param->wakeup_reason);
					break;
			}

		}
	}
	else{
			printf("argc=%d, argv[1]=%s, argv[2].\n", argc, argv[1],argv[2]);
			printf("error, try again!\n");
			goto _exit;
	}

	//printf("\ninterface=%s\n", interface);
_exit:
	free(pwowlan_ctrl_param);
	return ret;
}
