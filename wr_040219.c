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
#include <stdbool.h>

/** Maximum number of interfaces. */
#define MAX_ETH    32

/** Usage instructions. */
static const char *usage =
"Version 4.02.2019\n"
"usage: %s [-iwvarh] \n"
"	-i[interface], --interface	interface\n"
"       -w,--write    			write to register \n"
"       -v[value],--value      		value for writing to register \n"
"       -a[adress_of_PHY],--adress    	getting information about an exact PHY\n"
"       -r[register number],--register  getting register value\n"
"       -h,--help      			help and version information\n"
"\n"
"Examples:\n"
"-h			                                                                getting version and help information\n"
"-i[interface] -a [phy_adress_0..31]                                                    getting value of basic registers of the PHY\n"
"-i[interface] -a [phy_adress_0..31] -r [register_number]                               getting value of register_number\n"
"-i[interface] -w -v[value_for_register] -a [phy_adress_0..31] -r [register_number]     writing value_for_register to the register\n";

/** Struct fot ioctl calls. */
struct mii_data {
    uint16_t	phy_id;
    uint16_t	reg_num;
    uint16_t	val_in;
    uint16_t	val_out;
};

static struct option longopts[] = {
/**     {name  has_arg  *flag  val } */
	{"interface",	required_argument, 0, 'i'},	/** Name of interface.*/
	{"write",      no_argument, 0, 'w'},      /** Write to -n register. */
        {"adress",      required_argument, 0, 'n'},      /** Display number of PHYs on SMI. */
	{"register",      required_argument, 0, 'r'},      /** Display register's value. */
	{"value",	required_argument, 0, 'v'}, 	/** Value for register. */
	{"help",	no_argument, 0, 'h'},	/** Help. */
};

static unsigned int
	phy_addr = 0, /**< PHY adress */
	reg_num = 0, /**< Register number */
	value = 0; /**< Value for register */

static int skfd = -1; /**< File descriptor for socket */
static struct ifreq ifr; /**< Struct for ioctl call */
static struct if_nameindex *ifidx; /**< */

static int check_option_combination(bool flag_interface_set, bool flag_need_help,bool flag_write_to_register,bool flag_value_for_reg_set,bool flag_phy_addr_set,bool flag_register_number_set)
{
	bool flag_correct_args = false;
	if ((flag_interface_set) && (!flag_need_help) && (!flag_write_to_register) && (!flag_value_for_reg_set) && (!flag_phy_addr_set) && (!flag_register_number_set))//-i
                flag_correct_args = true;
        else if ((!flag_interface_set) && (flag_need_help) && (!flag_write_to_register) && (!flag_value_for_reg_set) && (!flag_phy_addr_set) && (!flag_register_number_set))//-h
                flag_correct_args = true;
        else if ((flag_interface_set) && (!flag_need_help) && (!flag_write_to_register) && (!flag_value_for_reg_set) && (flag_phy_addr_set) && (!flag_register_number_set))//-n
                flag_correct_args = true;
        else if ((flag_interface_set) && (!flag_need_help) && (!flag_write_to_register) && (!flag_value_for_reg_set) && (flag_phy_addr_set) && (flag_register_number_set))//-n -r
                flag_correct_args = true;
        else if ((flag_interface_set) && (!flag_need_help) && (flag_write_to_register) && (flag_value_for_reg_set) && (flag_phy_addr_set) && (flag_register_number_set))//-w -v -n -r
                flag_correct_args = true;
	
	if(flag_correct_args)
	       	return 0;
	else {
		printf("Incorrect arguments. -h for help.\n");
		//printf("%s \n", usage);
		return -1;
	}	
}

static int check_addr(int phy_addr)
{
	if ((phy_addr > 31) || (phy_addr < 0)){
                printf(" Incorrect PHY address. Available adresses: 0..31\n");
                return -1;
        }
	return 0;
}

static int check_register_number(int reg_num)
{
        if ((reg_num > 31) || (reg_num < 0)){
                printf(" Incorrect register. Available register numbers are: 0..31\n");
                return -1;
        }
        return 0;
}

static int check_value(int value)
{
	if ((value < 0) || (value > 0xFFFF)){
		printf(" Incorrect value for register. Availble values are: 0..0xFFFF");
		return -1;
	}
	return 0;
}

static int write_to_register(int skfd, int phy_addr, int reg_num, uint16_t reg_value)
{
	struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;

	mii->phy_id = phy_addr;
	mii->reg_num = reg_num;
	mii->val_in = reg_value;
	mii->val_out = 0;

	if (ioctl(skfd, SIOCSMIIREG, &ifr)<0){
                printf("Errno: %s\n", strerror(errno));
                return 1;
        } else {
                //TODO напечатать значение записанного регистра
                return 0;
        }

}

static int32_t read_register(int skfd, int phy_addr, int reg_num)
{
	struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;

	mii->phy_id = phy_addr;
        mii->reg_num = reg_num;
        mii->val_in = 1;
        mii->val_out = 0;
        if (ioctl(skfd, SIOCGMIIREG, &ifr)<0){
                printf("Impossible to read %d register. Errno: %s\n",reg_num, strerror(errno));
                return -1;
        } else {
                return mii->val_out;
        }
}

static int print_register(int skfd, int phy_addr, int reg_num)
{
	uint16_t temp;

	if ((temp = read_register(skfd, phy_addr, reg_num))<0){
                return -1;
        }
        printf("    Register %d = %x\n", reg_num, temp);
        return 0;
}

static int print_basic_registers(int skfd, int phy_addr)
{
	if (print_register(skfd, phy_addr, 0) < 0){
                return -1;
      	}
        if (print_register(skfd, phy_addr, 1) < 0){
                return -1;
        }
        return 0;
}

static int find_all_phy(int skfd)
{
	size_t i, count = 0;
	int32_t temp;

	printf(" Searching for PHY \n");
        printf(" Available PHYs: \n");
        printf(" PHY Adress    Identificator(2 and 3 registers)\n");

	for (i = 0; i < 32; i++){

      		printf("PHY ADRESS = %ld\n", i);

                if ((temp = read_register(skfd, i, 2)) < 0)
                	continue;

                if (temp == 0xFFFF){
                        printf(" No interface found on this adress.\n");
                        continue;
                }

                /* Если есть PHY по адресу i. */
                count++;

                if (print_register(skfd, i, 2) < 0){
                	continue;
                }
                if (print_register(skfd, i, 3) < 0){
                       	continue;
               	}
               	printf("\n");
    	}
      	printf("                     %ld interfaces found.\n", count);
	return 0;
}


int main(int argc, char **argv)
{
	int c;
	size_t i;
	bool flag_interface_set = false;
	bool flag_phy_addr_set = false;
       	bool flag_register_number_set = false;
	bool flag_write_to_register = false;
       	bool flag_value_for_reg_set = false;
       	bool flag_need_help = false;
	bool flag_correct_args = false;
	int32_t temp;
	char s[IFNAMSIZ];
        int err_num = 0;

	struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;

	while ((c = getopt_long(argc, argv, "i:wv:a:r:h", longopts, 0)) != EOF)
                switch (c) {
			case 'i': sprintf(s, optarg); flag_interface_set = true; break;
                	case 'a': phy_addr = strtol(optarg, NULL, 0); flag_phy_addr_set = true; break;
			case 'r': reg_num = strtol(optarg, NULL, 0); flag_register_number_set = true; break;
			case 'v': value = strtol(optarg, NULL, 0); flag_value_for_reg_set = true; break;
			case 'w': flag_write_to_register = true; break;
			case 'h': flag_need_help = true; break;
		}

	if (check_option_combination(flag_interface_set, flag_need_help, flag_write_to_register, flag_value_for_reg_set, flag_phy_addr_set, flag_register_number_set) < 0){
		close(skfd);
		return EXIT_FAILURE;
	}

	if ((check_addr(phy_addr) < 0) || (check_register_number(reg_num) < 0)){
                close(skfd);
		return EXIT_FAILURE;
        }

	if(check_value(value) < 0){
		close(skfd);
		return EXIT_FAILURE;
	}

	if (flag_need_help) {
		printf("%s \n", usage);
		return EXIT_SUCCESS;
	}

        if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("socket");
                return EXIT_FAILURE;
        }

	strncpy(ifr.ifr_name, s, sizeof(ifr.ifr_name));

	if (flag_write_to_register){ /* Write to register. */
		if (write_to_register(skfd, phy_addr, reg_num, value)){
			close(skfd);
			return EXIT_FAILURE;
		}
		close(skfd);
		return EXIT_SUCCESS;
	}

	/* PHY adress and register. */
	if (flag_phy_addr_set){/* Read from register. */
		if (flag_register_number_set){ //register is given
			if (print_register(skfd, phy_addr, reg_num) < 0){
				close(skfd);
				return EXIT_FAILURE;
			}
			close(skfd);
			return EXIT_SUCCESS;
		} else { //read basic registers
			if (print_basic_registers(skfd, phy_addr) < 0){
				close(skfd);
				return EXIT_FAILURE;
			}
			close(skfd);
			return EXIT_SUCCESS;
		}
	} else {
		if (find_all_phy(skfd) < 0){
			close(skfd);
			return EXIT_FAILURE;
		}
	}

        close (skfd);
        return EXIT_SUCCESS;
}
