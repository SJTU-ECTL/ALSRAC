\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{report}[Weekly Report Template]
\LoadClass{article}[9pt, a4paper]
\RequirePackage{amsmath, amssymb, amsfonts, graphicx}
\RequirePackage[top=1.0in, bottom=1.0in, left=1.0in, right=1.0in]{geometry}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% indent %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\RequirePackage{indentfirst}
\setlength{\parindent}{0em}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% end of indent %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% insert code %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\RequirePackage{listings}
\RequirePackage[usenames,dvipsnames]{color}
\definecolor{DarkGreen}{rgb}{0.0,0.4,0.0} % Comment color
\definecolor{highlight}{RGB}{255,251,204} % Code highlight color
\lstdefinestyle{Style1}{
language=c,
backgroundcolor=\color{highlight},
basicstyle=\footnotesize\ttfamily,
breakatwhitespace=false,
breaklines=true,
captionpos=b,
commentstyle=\usefont{T1}{pcr}{m}{sl}\color{DarkGreen},
deletekeywords={},
%escapeinside={\%},
firstnumber=1,
frame=single,
frameround=tttt,
keywordstyle=\color{Blue}\bf,
morekeywords={},
numbers=left,
numbersep=10pt,
numberstyle=\tiny\color{Gray},
rulecolor=\color{black},
showstringspaces=false,
showtabs=false,
stepnumber=5,
stringstyle=\color{Purple},
tabsize=2,
}
\newcommand{\insertcode}[2]{\begin{itemize}\item[]\lstinputlisting[caption=#2,label=#1,style=Style1]{#1}\end{itemize}} % The first argument is the script location/filename and the second is a caption for the listing
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% end of inserting code %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\endinput
