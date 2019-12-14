with import <nixpkgs> {};
mkShell {
  nativeBuildInputs = [ 
  bear subversion ffmpeg sox autoreconfHook intltool libxslt.bin docbook_xml_xslt libtool pkg-config ];
  buildInputs = map enableDebugging [ glib gtk3 ];
  checkInputs = [
    (python3.withPackages (ps: [ ps.dogtail ]))
  gnome3.gobject-introspection at-spi2-atk at-spi2-core
  tesseract imagemagick exiftool
  ];
  doCheck = true;
  shellHook = ''
    export XDG_DATA_DIRS=${gtk3}/share/gsettings-schemas/${gtk3.name}:${gsettings-desktop-schemas}/share/gsettings-schemas/${gnome3.gsettings-desktop-schemas.name}:${hicolor-icon-theme}/share:${gnome3.adwaita-icon-theme}/share:$XDG_DATA_DIRS
  '';
  G_DEBUG="fatal-criticals";
}
