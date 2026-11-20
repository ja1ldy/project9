# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

TINYX_SOURCES := \
  dix/atom.c dix/colormap.c dix/cursor.c dix/devices.c dix/dispatch.c \
  dix/dixfonts.c dix/fontstubs.c dix/embedded-font.c dix/embedded-font-data.c dix/dixutils.c \
  dix/events.c dix/extension.c dix/ffs.c dix/gc.c dix/globals.c \
  dix/glyphcurs.c dix/grabs.c dix/initatoms.c dix/lifecycle.c dix/main.c \
  dix/pixmap.c dix/privates.c dix/property.c dix/resource.c dix/swaprep.c \
  dix/swapreq.c dix/tables.c dix/window.c dix/strcasecmp.c mi/miarc.c \
  mi/mibank.c mi/mibitblt.c mi/micmap.c mi/micursor.c mi/midash.c \
  mi/midispcur.c mi/mieq.c mi/miexpose.c mi/mifillarc.c mi/mifillrct.c \
  mi/mifpolycon.c mi/migc.c mi/miglblt.c mi/mipointer.c mi/mipoly.c \
  mi/mipolycon.c mi/mipolygen.c mi/mipolypnt.c mi/mipolyrect.c mi/mipolyseg.c \
  mi/mipolytext.c mi/mipolyutil.c mi/mipushpxl.c mi/miregion.c mi/miscrinit.c \
  mi/mispans.c mi/misprite.c mi/mivaltree.c mi/miwideline.c mi/miwindow.c \
  mi/mizerarc.c mi/mizerclip.c mi/mizerline.c fb/fb24_32.c fb/fballpriv.c \
  fb/fbarc.c fb/fbbits.c fb/fbblt.c fb/fbbltone.c fb/fbcompose.c \
  fb/fbcopy.c fb/fbedge.c fb/fbfill.c fb/fbfillrect.c fb/fbfillsp.c \
  fb/fbgc.c fb/fbgetsp.c fb/fbglyph.c fb/fbimage.c fb/fbline.c \
  fb/fboverlay.c fb/fbpict.c fb/fbpixmap.c fb/fbpoint.c fb/fbpush.c \
  fb/fbscreen.c fb/fbseg.c fb/fbsetsp.c fb/fbsolid.c fb/fbstipple.c \
  fb/fbtile.c fb/fbtrap.c fb/fbutil.c fb/fbwindow.c Xi/allowev.c \
  Xi/chgdctl.c Xi/chgfctl.c Xi/chgkbd.c Xi/chgkmap.c Xi/chgprop.c \
  Xi/chgptr.c Xi/closedev.c Xi/devbell.c Xi/exevents.c Xi/extinit.c \
  Xi/getbmap.c Xi/getdctl.c Xi/getfctl.c Xi/getfocus.c Xi/getkmap.c \
  Xi/getmmap.c Xi/getprop.c Xi/getselev.c Xi/getvers.c Xi/grabdev.c \
  Xi/grabdevb.c Xi/grabdevk.c Xi/gtmotion.c Xi/listdev.c Xi/opendev.c \
  Xi/queryst.c Xi/selectev.c Xi/sendexev.c Xi/setbmap.c Xi/setdval.c \
  Xi/setfocus.c Xi/setmmap.c Xi/setmode.c Xi/stubs.c Xi/ungrdev.c \
  Xi/ungrdevb.c Xi/ungrdevk.c Xext/bigreq.c Xext/shape.c Xext/sleepuntil.c \
  Xext/sync.c Xext/xcmisc.c Xext/xtest.c Xext/xres.c Xext/saver.c \
  dbe/dbe.c dbe/midbe.c render/animcur.c render/filter.c render/glyph.c \
  render/miglyph.c render/miindex.c render/mipict.c render/mirect.c render/mitrap.c \
  render/mitri.c render/picture.c render/render.c render/renderedge.c randr/mirandr.c \
  randr/randr.c randr/rrcrtc.c randr/rrdispatch.c randr/rrinfo.c randr/rrmode.c \
  randr/rroutput.c randr/rrpointer.c randr/rrproperty.c randr/rrscreen.c randr/rrsdispatch.c \
  randr/rrxinerama.c xfixes/cursor.c xfixes/region.c xfixes/saveset.c xfixes/select.c \
  xfixes/xfixes.c damageext/damageext.c miext/damage/damage.c miext/shadow/shadow.c miext/shadow/shalloc.c \
  miext/shadow/shpacked.c miext/shadow/shplanar8.c miext/shadow/shplanar.c miext/shadow/shrot16pack_180.c miext/shadow/shrot16pack_270.c \
  miext/shadow/shrot16pack_270YX.c miext/shadow/shrot16pack_90.c miext/shadow/shrot16pack_90YX.c miext/shadow/shrot16pack.c miext/shadow/shrot32pack_180.c \
  miext/shadow/shrot32pack_270.c miext/shadow/shrot32pack_90.c miext/shadow/shrot32pack.c miext/shadow/shrot8pack_180.c miext/shadow/shrot8pack_270.c \
  miext/shadow/shrot8pack_90.c miext/shadow/shrot8pack.c miext/shadow/shrotate.c os/WaitFor.c os/connection.c \
  os/embedded-security.c os/host.c os/io.c os/memory.c os/transport.c \
  os/oscolor.c os/osinit.c os/utils.c os/log.c kdrive/src/kcmap.c \
  kdrive/src/kdrive.c kdrive/src/kinfo.c kdrive/src/kinput.c kdrive/src/kmap.c kdrive/src/kmode.c \
  kdrive/src/kshadow.c mi/miinitext.c fb/fbcmap.c kdrive/memory/api.c kdrive/memory/memory.c \
  kdrive/memory/meminit.c
