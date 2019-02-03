#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <glob.h>
#include <libgen.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <time.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/ethtool.h>
#include <linux/mdio.h>
#include <linux/sockios.h>

#define MAX_ETH    32   /* Maximum number of interfaces */

const char *usage =
"Version 3.02.2019\n"
"usage: %s [-wvnrh] \n"
"       -w,--write    			write to register \n"
"       -v[value],--value      		value for writing to register \n"
"       -n[adress_of_PHY],--number    	getting information about an exact PHY\n"
"       -r[register number],--register  getting register value\n"
"       -h,--help      			help and version information\n";

struct mii_data {
    __u16	phy_id;
    __u16	reg_num;
    __u16	val_in;
    __u16	val_out;
};

/* Table of known MII's */
static struct {
        u_short id1, id2;
        char *name;
} mii_id[] = {};

#define NMII (sizeof(mii_id)/sizeof(mii_id[0])) /* Count number of MII interfaces. */
#define NMEDIA (sizeof(media)/sizeof(media[0]))

struct option longopts[] = {
/*      {name  has_arg  *flag  val } */
	{"write",      0, 0, 'w'},      /* Write to -n register. */
        {"number",      1, 0, 'n'},      /* Display number of PHYs on SMI. */
	{"register",      1, 0, 'r'},      /* Display register's value. */
	{"value",	1, 0, 'v'}, 	/* Value for register. */
	{"help",	0, 0, 'h'},	/* Help. */
};

struct loc{
        char ifnam[32];
        uint16_t phy_id;
        uint16_t reg;
};

static unsigned int
	num = 0,
	regist = 0,
	value = 0;

static int skfd = -1;
static struct ifreq ifr;
struct if_nameindex *ifidx;

uint16_t *val = 0;


/*static int print_regist_value(int skfd, int num, int regist){
	struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;
	mii->phy_id = num;
	mii->reg_num = regist;

	mii->val_in = 1;
	mii->val_out = 0;

	if (ioctl(skfd, SIOCGMIIREG, &ifr)<0){
                printf("%ld\n ", num);
                printf("Errno: %s\n", strerror(errno));        
		return 1;
	} else {
               	if (mii->val_out){
//                	printf("PHY adress = %ld    ", num);
      	        	printf ("%d register = %x    ", regist,  mii->val_out);
		}
		return 0;
        }
}*/


int main(int argc, char **argv){
        int c, ret, errflag = 0;
	size_t i;
	int fn = 0, fr = 0, fw = 0, fv = 0, fh = 0, count = 0;
	char s[12];
        int err_num = 0;

        struct loc loc;

        struct loc *locp = &loc;
       
        struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;

        uint16_t *val = 0;

	int a = 0;

	while ((c = getopt_long(argc, argv, "wv:n:r:h", longopts, 0)) != EOF)
                switch (c) {
                	case 'n': num = atoi(optarg); fn = 1; break;
			case 'r': regist = atoi(optarg); fr = 1; break;
			case 'v': value = atoi(optarg); fv = 1; break;
			case 'w': fw = 1; break;
			case 'h': fh = 1; break;
		}

        /* Check for a few inappropriate option combinations */
	if (!((fh) || (fw && fv && fn && fr) || (fn && fr) || fn || (!(fw || fv || fn || fh || fr)))){
		printf(" Incorrect arguments.\n");
		return EXIT_FAILURE;
	}
	
	if (fh) {
		printf("%s \n", usage);
		return EXIT_SUCCESS;
	}

        if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("socket");
                return EXIT_FAILURE;
        }
	
	sprintf(s, "ens33");
        
	strncpy(ifr.ifr_name, s, sizeof(ifr.ifr_name));

        //print_regist_value(skfd, 38, 2);
        //return 0;


	if (fw){ /* Write to register. */
		locp->phy_id = num;
		mii->phy_id = locp->phy_id;
		
		mii->reg_num = regist;
		mii->val_in = value;//??????????
		mii->val_out = 1;
		
		if (ioctl(skfd, SIOCGMIIREG, &ifr)<0){
                	printf("Errno: %s\n", strerror(errno));
               		close (skfd);
			return EXIT_FAILURE;
		}

        	close (skfd);
        	return EXIT_SUCCESS;
	}

	/* PHY adress and register. */
	if (fn){/* Read from register. */
		//printf(" START read from register. ");
		
		/*if (fr)
			print_regist_value(skfd, num, regist);
		else {*/ /* Read basic registers (0 and 1). */
			/*print_regist_value(skfd, num, 0);
			print_regist_value(skfd, num, 1);
		}*/

		if ((num < 0) || (num > 31)){
			printf("Incorrect PHY adress.\n");
			close (skfd);
			return EXIT_FAILURE;
		}

		locp->phy_id = num;
		mii->phy_id = locp->phy_id;
		if (fr){
			mii->reg_num = regist;
			mii->val_in = 1;
                	mii->val_out = 0;
		
			if (ioctl(skfd, SIOCGMIIREG, &ifr)<0){
                 		printf("Errno %s\n", strerror(errno));
				close (skfd);
				return EXIT_FAILURE;
                	} else {
                        	if (mii->val_out){
                                	printf("PHY id = %d\n", num);
                                	printf ("Register %d = %x\n", regist, mii->val_out);
                        	}
                	}
		} else{ /* чтение базовых регистров */
			/* printf(" Register number is not given. \n"); */
			
			locp->phy_id = num;
			mii->phy_id = locp->phy_id;

			mii->reg_num = 0;
                        mii->val_in = 1;
                        mii->val_out = 0;

                        if (ioctl(skfd, SIOCGMIIREG, &ifr)<0){
                                printf("Errno %s\n", strerror(errno));
				close (skfd);
				return EXIT_FAILURE;
                        } else {
                                if (mii->val_out){
                                        printf("PHY adress = %d    ", num);
                                        printf ("Register 0 = %x    ", mii->val_out);
                                }
                        }
			
			mii->reg_num = 1;
                        mii->val_in = 1;
                        mii->val_out = 0;

                        if (ioctl(skfd, SIOCGMIIREG, &ifr)<0){
                                printf("Errno %s\n", strerror(errno));
				close (skfd);
				return EXIT_FAILURE;
                        } else {
                                if (mii->val_out){
                                        //printf("PHY adress = %d    ", num);
                                        printf ("Register 1 = %x\n", mii->val_out);
                                }
                        }
		}
	} else {
		printf(" Searching for PHY \n");

		printf(" Available PHYs: \n");

		printf("PHY Adress    Identificator(2 and 3 registers)\n");
		for (i = 0; i < 32; i++){
			
			/*printf("PHY adress = %d        ", i);
			if (!((print_regist_value(skfd, i, 2)) || (print_regist_value(skfd, i, 3)) == 0))
				printf(" This adress is not available." );
			printf("\n");*/
                	locp->phy_id = i;
	                locp->reg = 2;
	
        		mii->phy_id = locp->phy_id;
			mii->reg_num = locp->reg;
                	mii->val_in = 1;
            		mii->val_out = 0;

	                if (ioctl(skfd, SIOCGMIIREG, &ifr)<0){
				printf("Errno: %s\n", strerror(errno));
	               	} else {
				if (mii->val_out){
					printf("    %ld    ", i);
					printf ("      %x    ", mii->val_out);
				}
			}
			
			locp->reg = 3;

			mii->reg_num = locp->reg;
                        mii->val_in = 1;
                        mii->val_out = 0;

                        if (ioctl(skfd, SIOCGMIIREG, &ifr)<0){
                                printf("Errno: %s\n", strerror(errno));
                        } else {
                                if (mii->val_out){
					count++;
                                        printf (" %x\n", mii->val_out);
                                }
                        }

		}
		printf(" %d interfaces found. \n", count);
	}

        close (skfd);
        return EXIT_SUCCESS;
}
