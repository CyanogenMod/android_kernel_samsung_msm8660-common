
#ifndef __SPI_DRV_H
#define __SPI_DRV_H

//#define CONFIG_VIDEO_SPI_DEBUG


/* Debug macro */
#define SPI_DEBUG(fmt, ...)                                     \
	do {                                                    \
	        printk(                               \
	                "%s: " fmt, __func__, ##__VA_ARGS__);   \
	} while(0)


#define SPI_WARN(fmt, ...)                                      \
	do {                                                    \
	        printk(KERN_WARNING                             \
	                fmt, ##__VA_ARGS__);                    \
	} while (0)

#define SPI_ERROR(fmt, ...)                                     \
	do {                                                    \
	        printk(KERN_ERR                                 \
	                "%s: " fmt, __func__, ##__VA_ARGS__);   \
        } while (0)



#ifdef CONFIG_VIDEO_SPI_DEBUG
#define spi_dbg(fmt, ...)               SPI_DEBUG(fmt, ##__VA_ARGS__)
#else
#define spi_dbg(fmt, ...)
#endif
        
#define spi_warn(fmt, ...)              SPI_WARN(fmt, ##__VA_ARGS__)
#define spi_err(fmt, ...)               SPI_ERROR(fmt, ##__VA_ARGS__)


void nmi326_spi_read_chip_id(void);
int nmi326_spi_init(void);
void  nmi326_spi_exit( void );
int nmi326_spi_read(u8 *buf, size_t len);
int nmi326_spi_write(u8 *buf, size_t len);

#endif	//__SPI_DRV_H
