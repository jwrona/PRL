#####
#file: Makefile
#author: Jan Wrona
#email: <xwrona00@stud.fit.vutbr.cz>
#date: 24.4.2015
#project: Mesh Multiplication, PRL
#####

NAME=mm

latex: $(NAME).tex
	latex $(NAME).tex
	latex $(NAME).tex
	dvips $(NAME).dvi
	ps2pdf $(NAME).ps

clean:
	rm -f $(NAME).{aux,log,out,toc,bbl,blg,dvi,ps}
