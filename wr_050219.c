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

/** Максимальное количество подключенных PHY. */
#define MAX_ETH    32

/** Инструкции по использованию. */
static const char *usage =
"Version 5.02.2019\n"
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

/** Структура для системного вызова ioctl(). */
struct mii_data {
    uint16_t	phy_id; 
    uint16_t	reg_num;
    uint16_t	val_in; /**< Значение, которое будет записано в регистр. */
    uint16_t	val_out; /**< Значение, прочитанное из регистра. */  
};

static struct option longopts[] = {
/**     {name  has_arg  *flag  val } */
	{"interface",	required_argument, 0, 'i'},	/**< Название сетевого интерфейса.*/
	{"write",      no_argument, 0, 'w'},      /**< Если требуется записать в регистр. */
        {"adress",      required_argument, 0, 'n'},      /**< Адрес PHY. */
	{"register",      required_argument, 0, 'r'},      /**< Номер регистра. */
	{"value",	required_argument, 0, 'v'}, 	/**< Значение для записи в регистр. */
	{"help",	no_argument, 0, 'h'},	/**< Help. */
};

static unsigned int
	phy_addr = 0, /**< Адрес PHY. */
	reg_num = 0, /**< Номер регистра. */
	value = 0; /**< Значение, которое будет записано в регистр. */

static int skfd = -1; /**< Файловый дескриптор, использующийся для передачи сокета. */
static struct ifreq ifr; /**< Структура для системного вызова ioctl(). */
static struct if_nameindex *ifidx; /**< Сюда будет записано имя сетевого интерфейса. */

/** 
 * Проверка правильности введённых опций.
 * flag_correct_args принимает значение true, если комбинация опций правильная.
 * На вход принимаются значения флагов, каждый из которых отвечает за одну опцию.
 * Если опция была введена пользователем, значит соответствующий флаг равен true.
 * Если не была введена - тогда ralse.
 */
static int check_option_combination(bool flag_interface_set, bool flag_need_help,bool flag_write_to_register,bool flag_value_for_reg_set,bool flag_phy_addr_set,bool flag_register_number_set)
{
	bool flag_correct_args = false;
	
	/** Если введённые опции: -i. */
	if ((flag_interface_set) && (!flag_need_help) && (!flag_write_to_register) && (!flag_value_for_reg_set) && (!flag_phy_addr_set) && (!flag_register_number_set))
                flag_correct_args = true;
	
	/** Если введённые опции: -h */
        else if ((!flag_interface_set) && (flag_need_help) && (!flag_write_to_register) && (!flag_value_for_reg_set) && (!flag_phy_addr_set) && (!flag_register_number_set)) 
                flag_correct_args = true;
	
	/** Если введённые опции: -i -a */
        else if ((flag_interface_set) && (!flag_need_help) && (!flag_write_to_register) && (!flag_value_for_reg_set) && (flag_phy_addr_set) && (!flag_register_number_set))
                flag_correct_args = true;
        
	/** Если введённые опции: -i -a -r */
	else if ((flag_interface_set) && (!flag_need_help) && (!flag_write_to_register) && (!flag_value_for_reg_set) && (flag_phy_addr_set) && (flag_register_number_set))
                flag_correct_args = true;
       
       /** Если введённые опции: -i -w -v -a -r */
	else if ((flag_interface_set) && (!flag_need_help) && (flag_write_to_register) && (flag_value_for_reg_set) && (flag_phy_addr_set) && (flag_register_number_set))
                flag_correct_args = true;
	
	if(flag_correct_args)
	       	return 0;
	else {
		printf("Incorrect arguments. -h for help.\n");
		//printf("%s \n", usage);
		return -1;
	}	
}

/** Проверка, введён ли адрес PHY правильно. */
static int check_addr(int phy_addr)
{
	if ((phy_addr > 31) || (phy_addr < 0)){
                printf(" Incorrect PHY address. Available adresses: 0..31\n");
                return -1;
        }
	return 0;
}

/** Проверка, введён ли номер регистра правильно. */
static int check_register_number(int reg_num)
{
        if ((reg_num > 31) || (reg_num < 0)){
                printf(" Incorrect register. Available register numbers are: 0..31\n");
                return -1;
        }
        return 0;
}

/** Проверка, введено ли значение для записи в регистр правильно. */
static int check_value(int value)
{
	if ((value < 0) || (value > 0xFFFF)){
		printf(" Incorrect value for register. Availble values are: 0..0xFFFF");
		return -1;
	}
	return 0;
}

/**
 * Функция записывает значение reg_value в регистр с номером reg_num,
 * который находится в PHY с адресом phy_addr.
 * Используется файловый дескриптор skfd.
 */
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

/** 
 * Чтение значения регистра с номером reg_num из PHY с адресом phy_addr.
 * Используется файловый дескриптор skfd.
 */
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

/**
 * Печать значения регистра с номером reg_num,
 * который находится в PHY с адресом phy_addr.
 * Используется файловый дескриптор skfd.
 */
static int print_register(int skfd, int phy_addr, int reg_num)
{
	int32_t temp;

	/** Возвращаемое значение равно -1, если регистр не может быть прочитан. */
	if ((temp = read_register(skfd, phy_addr, reg_num))<0){
                return -1;
        }
        printf("    Register %d = %x\n", reg_num, temp);
        return 0;
}

/**
 * Печать значений двух базовых регистров из PHY с адресом phy_addr. 
 * -1 в случае ошибки.
 */
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

/**
 * Поиск всех PHY на шине SMI.
 * Печать адресов и идентификаторов(значений регистров 2 и 3) найденных PHY.
 */ 
static int find_all_phy(int skfd)
{
	size_t i, count = 0;/**< count - количество найденных PHY. */
	int32_t temp;

	printf(" Searching for PHY \n");
        printf(" Available PHYs: \n");
        printf(" PHY Adress    Identificator(2 and 3 registers)\n");

	/** Проверка всевозможных адресов. */
	for (i = 0; i < 32; i++){

      		printf("PHY ADRESS = %ld\n", i);

		/** Если второй регистр не может быть прочитан - тогда переход на следующую итерацию цикла. */
                if ((temp = read_register(skfd, i, 2)) < 0)
                	continue;

		/** Если значение второго регистра равно 0xFFFF, тогда по этому адресу PHY не найден. */
                if (temp == 0xFFFF){
                        printf(" No interface found on this adress.\n");
                        continue;
                }

                /** Если PHY найден. */
                count++;

		/** 
		 * Печать регистров 2 и 3 - идентификаторов PHY.
		 */
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
	int c; /**< Переменная для хранения введённых опций. */
	size_t i; /**< Счётчик для циклов. */
	
	/**
	 * Флаги. Будут показывать, какие опции введены.
	 */
	bool flag_interface_set = false; /**< -i интерфейс. */
	bool flag_phy_addr_set = false; /**< -a адрес PHY. */
       	bool flag_register_number_set = false; /**< -r номер регистра. */
	bool flag_write_to_register = false;/**< -w запись в регистр. */
       	bool flag_value_for_reg_set = false;/**< -v значение. */
       	bool flag_need_help = false;/**< -h help. */

	/** Флаг = 1 если введённая комбинация опций правильная. 0 - если неправильная. */
	bool flag_correct_args = false;

	int32_t temp;/** Переменная, куда будет записываться значение регистра.*/
	
	char s[IFNAMSIZ];/** Имя интерфейса. */


	while ((c = getopt_long(argc, argv, "i:wv:a:r:h", longopts, 0)) != EOF)
                switch (c) {
			case 'i': sprintf(s, optarg); flag_interface_set = true; break;
                	case 'a': phy_addr = strtol(optarg, NULL, 0); flag_phy_addr_set = true; break;
			case 'r': reg_num = strtol(optarg, NULL, 0); flag_register_number_set = true; break;
			case 'v': value = strtol(optarg, NULL, 0); flag_value_for_reg_set = true; break;
			case 'w': flag_write_to_register = true; break;
			case 'h': flag_need_help = true; break;
		}

	/** Проверка комбинации опций. */
	if (check_option_combination(flag_interface_set, flag_need_help, flag_write_to_register, flag_value_for_reg_set, flag_phy_addr_set, flag_register_number_set) < 0){
		return EXIT_FAILURE;
	}

	/** Проверка введённого адреса. */
	if ((check_addr(phy_addr) < 0) || (check_register_number(reg_num) < 0)){
		return EXIT_FAILURE;
        }

	/** Проверка введённого значения для регистра. */
	if(check_value(value) < 0){
		return EXIT_FAILURE;
	}

	/** Вывод инструкций. */
	if (flag_need_help) {
		printf("%s \n", usage);
		return EXIT_SUCCESS;
	}

	/** socket() создаёт сокет и присваивает skfd значение дескриптора, который был присвоен этому сокету. */
        if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("socket");
                return EXIT_FAILURE;
        }

	/** Запись имени интерфейса в структуру. */
	strncpy(ifr.ifr_name, s, sizeof(ifr.ifr_name));

	/**
	 * ***************************************************************************************************
	 * Далее в зависимости от установленных флагов выполняются действия чтения, записи и вывода на экран.
	 * ***************************************************************************************************
	 */

	/** Записать в регистр. */
	if (flag_write_to_register){
		if (write_to_register(skfd, phy_addr, reg_num, value)){
			close(skfd);
			return EXIT_FAILURE;
		}
		close(skfd);
		return EXIT_SUCCESS;
	}

	/** Если введён номер PHY: */
	if (flag_phy_addr_set){
		/** Если введены имя интерфейса, адрес PHY и номер регистра, то печатается значение этого регистра. */
		if (flag_register_number_set){
			if (print_register(skfd, phy_addr, reg_num) < 0){
				close(skfd);
				return EXIT_FAILURE;
			}
			close(skfd);
			return EXIT_SUCCESS;
		/** Если введён только адрес PHY и имя интерфейса. */
		} else {
			if (print_basic_registers(skfd, phy_addr) < 0){
				close(skfd);
				return EXIT_FAILURE;
			}
			close(skfd);
			return EXIT_SUCCESS;
		}
	/** 
	 * Если введено только имя интерфейса.
	 * Выполняется поиск всех PHY, подключенных к SMI, печать их адресов и идентификаторов.
	 * Выводится количество подключённых PHY.
	 */
	} else {
		if (find_all_phy(skfd) < 0){
			close(skfd);
			return EXIT_FAILURE;
		}
	}

        close (skfd);
        return EXIT_SUCCESS;
}
