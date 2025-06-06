In-line Scanners:
  * [x] _under-emph_
  * [x] *star-emph*
  * [x] __under-strong__
  * [x] **star-strong**
  * [x] ___under-strong-emph___
  * [x] ***star-strong-emph***
  * [ ] super^script^
  * [ ] sub~script~
  * [ ] ~~strikethrough~~
  * [ ] [text span]{.underline}
  * [ ] [web link](https://www.google.com)
  * [ ] `code block`
  * [ ] `` `literal looking` ``

When considering underscore and star syntax together, the parse_inline function
may need to consider symbols prior. (more maybe just backtrack?)
