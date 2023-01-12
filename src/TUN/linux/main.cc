#include <linux/if.h>
#include <linux/if_tun.h>

int tun_alloc(char *dev) {
  struct ifreq ifr;
  int fd, err;

  if( (fd = open("/dev/net/tun", O_RDWR)) < 0 )
      return tun_alloc_old(dev);

  memset(&ifr, 0, sizeof(ifr));

  /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
    *        IFF_TAP   - TAP device
    *
    *        IFF_NO_PI - Do not provide packet information
    */
  ifr.ifr_flags = IFF_TUN;
  if( *dev )
      strncpy(ifr.ifr_name, dev, IFNAMSIZ);

  if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
      close(fd);
      return err;
  }
  strcpy(dev, ifr.ifr_name);
  return fd;
}