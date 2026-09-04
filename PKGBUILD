pkgname=omsticker
pkgver=0.9.0
pkgrel=1
pkgdesc="USB creator for Omarchy that writes an ISO and formats leftover space"
arch=('x86_64')
url="https://github.com/paulogeyer/omsticker"
license=('MIT')
depends=('qt6-base' 'qt6-svg' 'udisks2' 'util-linux' 'polkit' 'exfatprogs' 'dosfstools' 'e2fsprogs')
optdepends=(
    'ntfs-3g: NTFS leftover filesystem'
    'btrfs-progs: Btrfs leftover filesystem'
    'xfsprogs: XFS leftover filesystem'
    'f2fs-tools: F2FS leftover filesystem'
)
makedepends=('qt6-base')
source=("git+${url}.git#tag=v0.9")
sha256sums=('SKIP')

build() {
    cd "${srcdir}/${pkgname}"
    qmake6 PREFIX=/usr omsticker.pro
    make
}

package() {
    cd "${srcdir}/${pkgname}"
    make INSTALL_ROOT="${pkgdir}" install
    install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}
