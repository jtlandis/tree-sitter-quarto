In-line Scanners:
  * [x] _under-emph_
  * [x] *star-emph*
  * [x] __under-strong__
  * [x] **star-strong**
  * [x] ___under-strong-emph___
  * [x] ***star-strong-emph***
  * [x] super^script^
  * [x] sub~script~
  * [x] ~~strikethrough~~
  * [x] [text span]{.underline}
  * [x] [web link](https://www.google.com)
  * [x] `verbatim`
  * [x] `` `literal looking` ``
  * [ ] citation links
  * [x] equations
  * [ ] definitions
  * [ ] yaml header
  * [ ] footnotes

Other scanner options:
 - [ ] code block syntax
 - [ ] div style blocks "::: callout  :::"
 - [ ] list items
       - [ ] 
 - [ ] indentations
 - [ ] check boxes
 - [ ] pipe/other tables???
 - [ ] block eqauations

When considering underscore and star syntax together, the parse_inline function
may need to consider symbols prior. (more maybe just backtrack?)


NOTES:

The quarto docs state that attributes are not allowed unless they are in a specific order. I am finding this not to be the case for my quarto version (1.7.31).


````qmd

<!-- should not work -->
[some text]{.mark #id}

````
