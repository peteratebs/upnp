docxx -H -d controlpointhtml cpupnp.dxx
docxx -p -u -t -o controlpointReferenceGuide.tex cpupnp.dxx
copy arrow20.gif controlpointhtml\icon1.gif
copy arrow20.gif controlpointhtml\icon2.gif
pdflatex controlpointReferenceGuide.tex