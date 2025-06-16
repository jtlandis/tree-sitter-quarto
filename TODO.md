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
  * [ ] [text span]{.underline}
  * [ ] [web link](https://www.google.com)
  * [ ] `code block`
  * [ ] `` `literal looking` ``
  * [ ] citation links

Other scanner options:
 - [ ] code block syntax
 - [ ] indentations
 - [ ] list items
 - [ ] pipe/other tables???

When considering underscore and star syntax together, the parse_inline function
may need to consider symbols prior. (more maybe just backtrack?)
