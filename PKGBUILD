# PKGBUILD file author: deadYokai
pkgname=rewards-theater-obs-git
_pkgname=RewardsTheater
pkgver=1.1.5
pkgrel=1
pkgdesc="An OBS plugin that lets your viewers redeem videos or sounds on stream via channel points."
arch=('x86_64')
url="https://github.com/gottagofaster236/RewardsTheater"
license=('GPL-3.0-only')

depends=('obs-studio')
makedepends=(
	'git'
	'ccache'
	'cmake'
	'extra-cmake-modules'
	'libx11'
	'boost'
	'qt6-base'
	'qt6-svg'
)

provides=("${pkgname%-git}")
conflicts=("${pkgname%-git}")

source=("git+https://github.com/gottagofaster236/RewardsTheater")
sha256sums=('SKIP')

pkgver() {
	cd "${srcdir}/${_pkgname}"
	git describe --tags | sed 's/^v//;s/-/+/g'
}

build() {
	cd "${srcdir}/${_pkgname}"

	# https://github.com/gottagofaster236/RewardsTheater/issues/16
	export CXXFLAGS+=" -Wno-error=deprecated-declarations"
	
	cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DLINUX_PORTABLE=OFF
	cmake --build build
}

package() {
	cd "${srcdir}/${_pkgname}"
	DESTDIR="${pkgdir}" cmake --install build

	install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}

