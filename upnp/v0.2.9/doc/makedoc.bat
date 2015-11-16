docxx -H -d devicehtml upnp.dxx
docxx -p -u -t -o DeviceReferenceGuide.tex upnp.dxx
copy arrow20.gif devicehtml\icon1.gif
copy arrow20.gif devicehtml\icon2.gif
pdflatex DeviceReferenceGuide.tex