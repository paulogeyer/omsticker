pkgname=omsticker
pkgver=0.9.1
pkgrel=1
pkgdesc="USB creator for Omarchy that writes an ISO and formats leftover space"
arch=('x86_64')
url="https://github.com/paulogeyer/omsticker"
license=('MIT')
depends=('qt6-base' 'qt6-svg' 'udisks2' 'util-linux' 'exfatprogs' 'dosfstools' 'e2fsprogs')
optdepends=(
    'ntfs-3g: NTFS leftover filesystem'
    'btrfs-progs: Btrfs leftover filesystem'
    'xfsprogs: XFS leftover filesystem'
    'f2fs-tools: F2FS leftover filesystem'
)
makedepends=('qt6-base')
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$srcdir/$pkgname-$pkgver"
    qmake6 PREFIX=/usr omsticker.pro
    make
}

package() {
    cd "$srcdir/$pkgname-$pkgver"
    make INSTALL_ROOT="$pkgdir" install
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
