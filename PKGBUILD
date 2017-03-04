# Contributor: Damir Perisa <damir.perisa@bluewin.ch>
# Maintainer: Oliver Kuster<olivervbk@gmail.com>

pkgname=hdup-git
pkgver=2.0.git
pkgrel=4
pkgdesc="The little, spiffy, backup tool"
arch=(i686 x86_64)
license=('GPL2')
depends=('coreutils' 'mcrypt' 'openssh' 'gnupg' 'gzip' 'bzip2' 'lzop' 'glibc' 'glib2')
backup=('etc/hdup/hdup.conf')
source=("${pkgname%-git}::git+http://github.com/ctizen/hdup.git")

build() {
  cd $srcdir/${pkgname%-git}
  ./configure --prefix=/usr --sysconfdir=/etc --mandir=/usr/share/man
  make
}

package() {
  cd $srcdir/${pkgname%-git}
  make DESTDIR=$pkgdir install

  chmod a+r $pkgdir/etc/hdup/hdup.conf
  chmod a+r $pkgdir/etc/hdup/postrun-warn-user

  mv $pkgdir/usr/{s,}bin
}

