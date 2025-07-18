/**
 * @file Quarto markdown format
 * @author jtlandis <jtlandis314@gmail.com>
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: "quarto",

  extras: ($) => [
    // The below symbol matches any whitespace character (spaces, tabs, line breaks, etc.)
    /\s+/,
    $.comment,
  ],

  externals: ($) => [
    $.empty,
    $.line_start,
    $.line_end,
    $.indent,
    $.dedent,
    $._emph_star_start,
    $._emph_star_end,
    $._emph_under_start,
    $._emph_under_end,
    $._strong_star_start,
    $._strong_star_end,
    $._strong_under_start,
    $._strong_under_end,
    $.superscript_start,
    $.superscript_end,
    $.subscript_start,
    $.subscript_end,
    $.strike_start,
    $.strike_end,
    $.bracket_start,
    $.bracket_end,
    $.link_start,
    $.link_end,
    $.curly_start,
    $.curly_end,
    $.attr_id,
    $.attr_class,
    $.attr_key,
    $.attr_value,
    $.verbatim,
    $.math,
    $.ordered,
    $.unordered,
    $.list_start,
    $.list_item_end,
    $.div_start,
    $.div_end,
    $._no_parse,
    $._unused_error,
  ],

  rules: {
    source_file: ($) => seq(repeat($.line_end), repeat($._section)),

    comment: ($) => token(seq("<!--", /.*/, "-->")),

    _yaml: ($) => choice(),
    paragraph: ($) =>
      prec.right(
        3,
        seq(
          optional($._new_line_start),
          $._line_content,
          repeat(seq($._new_line_end, $._new_line_start, $._line_content)),
          $.paragraph_end,
        ),
      ),
    line_break: ($) =>
      prec.right(
        2,
        choice(
          seq(token.immediate("\\"), $.line_end),
          seq(token.immediate("  "), $.line_end),
        ),
      ),
    paragraph_end: ($) =>
      prec.right(4, choice(repeat1($.line_end), seq($.line_end, $.empty))),
    _line_content: ($) =>
      repeat1(
        choice(
          $.strong,
          $.emph,
          $.word,
          $.puncuation,
          $.literal,
          $.symbols,
          $.superscript,
          $.subscript,
          $.striketrhough,
          $.hyperlink,
          $.span,
          $.inline_math,
          $.display_math,
          seq($.verbatim, optional($.curly_attrs)),
          alias($._no_parse, $.literal),
        ),
      ), //, $.whitespace)), //prec(1, repeat1(choice($.word, $.whitespace))),
    _new_line_end: ($) => prec(20, choice($.line_end, $.line_break)),
    _new_line_start: ($) => seq($.line_start, repeat($.indent)),
    _empty_line: ($) => seq($._new_line_start, $._new_line_end),
    line: ($) => prec.right(2, seq($._line_content, $._new_line_end)),
    _literal_line: ($) => prec.right(2, seq($._line_content, $.line_end)),
    _complete_line: ($) => prec.right(2, seq($._new_line_start, $.line)), //prec.right(seq($._line, optional($.line_end))),
    inline_math: ($) => seq("$", $.math, "$"),
    display_math: ($) => seq("$$", $.math, "$$"),
    word: ($) => /[\p{L}\p{N}]+/,
    puncuation: ($) =>
      choice(
        $.period,
        $.comma,
        $.question,
        $.exclamation,
        $.colon,
        $.semi_colon,
        $.quotation,
      ),
    period: ($) => ".",
    comma: ($) => ",",
    exclamation: ($) => "!",
    question: ($) => "?",
    colon: ($) => ":",
    semi_colon: ($) => ";",
    quotation: ($) => choice($.single_quote, $.double_quote),
    single_quote: ($) => "'",
    double_quote: ($) => '"',
    symbols: ($) => /[@#\$%\^\&\*\(\)_\+\=\-/><~\\\{\}]/,
    literal: ($) => prec(10, /\\[@#\$%\^\&\*\(\)_\+\=\-/><~\\ ]/),
    // content: ($) => repeat1($.paragraph),
    _block_content: ($) =>
      choice($.paragraph, seq(optional($._new_line_start), $.list), $.div),
    content: ($) => prec.right(repeat1($._block_content)),
    _section: ($) => choice($.heading, $.content),
    heading: ($) =>
      prec(
        5,
        seq(
          $.line_start,
          choice(
            $.heading_1,
            $.heading_2,
            $.heading_3,
            $.heading_4,
            $.heading_5,
            $.heading_6,
          ),
        ),
      ),
    heading_1: ($) => prec.right(seq("#", $._line_content, repeat($.line_end))),
    heading_2: ($) =>
      prec.right(seq("##", $._line_content, repeat($.line_end))),
    heading_3: ($) =>
      prec.right(seq("###", $._line_content, repeat($.line_end))),
    heading_4: ($) =>
      prec.right(seq("####", $._line_content, repeat($.line_end))),
    heading_5: ($) =>
      prec.right(seq("#####", $._line_content, repeat($.line_end))),
    heading_6: ($) =>
      prec.right(seq("######", $._line_content, repeat($.line_end))),

    emph: ($) => choice(prec(3, $._emph_star), prec(3, $._emph_under)),
    _emph_star: ($) =>
      seq(
        alias($._emph_star_start, $.emph_start),
        $._line_content,
        alias($._emph_star_end, $.emph_end),
      ),
    _emph_under: ($) =>
      seq(
        alias($._emph_under_start, $.emph_start),
        $._line_content,
        alias($._emph_under_end, $.emph_end),
      ),
    strong: ($) => choice(prec(3, $._strong_star), prec(3, $._strong_under)),
    // strong: ($) => $._strong_star,
    _strong_star: ($) =>
      seq(
        alias($._strong_star_start, $.strong_start),
        $._line_content,
        alias($._strong_star_end, $.strong_end),
      ),
    _strong_under: ($) =>
      seq(
        alias($._strong_under_start, $.strong_start),
        $._line_content,
        alias($._strong_under_end, $.strong_end),
      ),
    superscript: ($) =>
      seq($.superscript_start, $._line_content, $.superscript_end),
    subscript: ($) => seq($.subscript_start, $._line_content, $.subscript_end),
    striketrhough: ($) => seq($.strike_start, $._line_content, $.strike_end),
    hyperlink: ($) =>
      seq(
        $.bracket_start,
        repeat(prec.left(seq($._line_content, repeat($.line_end)))),
        $.bracket_end,
        $.link_start,
        $.link,
        $.link_end,
        optional($.curly_attrs),
      ),
    link: ($) => choice($.empty, /[^)]+/),
    span: ($) =>
      seq(
        $.bracket_start,
        repeat(prec.left(seq($._line_content, repeat($.line_end)))),
        $.bracket_end,
        $.curly_attrs,
      ),
    curly_attrs: ($) => seq($.curly_start, $.attrs, $.curly_end),
    attrs: ($) =>
      repeat1(choice($.empty, $.attr_id, $.attr_class, $.attr_keyvalue)),
    // attr_id: ($) => seq("#", $.attr_id),
    // attr_class_: ($) => seq($.period, $.attr_class),
    attr_keyvalue: ($) =>
      seq(field("key", $.attr_key), "=", field("value", $.attr_value)),

    list_item: ($) =>
      prec.right(
        4,
        seq(
          choice($.ordered, $.unordered),
          alias($._no_parse, $.spacing),
          $.content,
          optional($.list_item_end),
          // choice($.content, alias($._literal_line, $.line)),
        ),
      ),
    // _list_item: ($) => choice($.list, $.list_item),
    list: ($) =>
      prec.right(
        seq(
          $.list_start,
          $.list_item,
          repeat(seq($._new_line_start, $.list_item)),
          optional($.dedent),
        ),
      ),

    div: ($) =>
      prec.right(
        seq(
          optional($._new_line_start),
          $.div_start,
          choice($.curly_attrs, $.attr_class),
          repeat($._new_line_end),
          $._new_line_end,
          $.content,
          optional(seq($._new_line_start, $.div_end, $._new_line_end)),
        ),
      ),
  },

  conflicts: ($) => [
    // [$.content],
    // [$.paragraph],
    // [$.paragraph, $.line],
    // [$.paragraph, $.word],
  ],
});
