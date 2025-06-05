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
    $._line_start,
    $.line_end,
    $._emph_star_start,
    $._emph_star_end,
    $._emph_under_start,
    $._emph_under_end,
    $._strong_star_start,
    $._strong_star_end,
    $._strong_under_start,
    $._strong_under_end,
    $._no_parse,
    $._unused_error,
  ],

  rules: {
    source_file: ($) => repeat($._section),

    comment: ($) => token(seq("<!--", /.*/, "-->")),

    _yaml: ($) => choice(),
    paragraph: ($) => prec.right(3, seq(repeat1($._line), $.paragraph_end)),
    line_break: ($) =>
      prec.right(
        2,
        choice(
          seq(token.immediate("\\"), $.line_end),
          seq(token.immediate("  "), $.line_end),
        ),
      ),
    paragraph_end: ($) =>
      prec.right(
        4,
        repeat1($.line_end),
        // choice(
        //   seq($.line_break, repeat1($.line_end)),
        //   seq($.line_end, repeat1($.line_end)),
        // ),
      ),
    _line_content: ($) =>
      repeat1(
        choice(
          $.strong,
          $.emph,
          $.word,
          $.puncuation,
          $.literal,
          $.symbols,
          alias($._no_parse, $.literal),
        ),
      ), //, $.whitespace)), //prec(1, repeat1(choice($.word, $.whitespace))),
    _line: ($) =>
      prec.right(
        2,
        seq($._line_start, $._line_content, choice($.line_break, $.line_end)),
      ), //prec.right(seq($._line, optional($.line_end))),
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
    symbols: ($) => /[@#\$%\^\&\*\(\)_\+\=\-/><~\\]/,
    literal: ($) => prec(10, /\\[@#\$%\^\&\*\(\)_\+\=\-/><~\\ ]/),
    content: ($) => prec.right(seq(repeat($.line_end), repeat1($.paragraph))),
    _section: ($) =>
      prec.right(choice(seq($.heading, $.content), $.heading, $.content)),
    heading: ($) =>
      seq(
        $._line_start,
        choice(
          $.heading_1,
          $.heading_2,
          $.heading_3,
          $.heading_4,
          $.heading_5,
          $.heading_6,
        ),
      ),
    heading_1: ($) => prec.right(seq("#", $._line, repeat($.line_end))),
    heading_2: ($) => prec.right(seq("##", $._line, repeat($.line_end))),
    heading_3: ($) => prec.right(seq("###", $._line, repeat($.line_end))),
    heading_4: ($) => prec.right(seq("####", $._line, repeat($.line_end))),
    heading_5: ($) => prec.right(seq("#####", $._line, repeat($.line_end))),
    heading_6: ($) => prec.right(seq("######", $._line, repeat($.line_end))),

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
    _emph_content: ($) =>
      prec.right(
        repeat1(
          seq(
            repeat1(
              choice($.word, $.puncuation, $.literal, $.symbols, $.strong),
            ),
            optional(choice($.line_break, $.line_end)),
          ),
        ),
      ),
    strong: ($) => choice(prec(3, $._strong_star), prec(3, $._strong_under)),
    // strong: ($) => $._strong_star,
    _strong_star: ($) =>
      seq(
        alias($._strong_star_start, $.strong_start),
        $._strong_content,
        alias($._strong_star_end, $.strong_end),
      ),
    _strong_under: ($) =>
      seq(
        alias($._strong_under_start, $.strong_start),
        $._strong_content,
        alias($._strong_under_end, $.strong_end),
      ),
    _strong_content: ($) =>
      prec.right(
        repeat1(
          seq(
            repeat1(choice($.word, $.puncuation, $.literal, $.symbols, $.emph)),
            optional(choice($.line_break, $.line_end)),
          ),
        ),
      ),
  },

  conflicts: ($) => [
    [$._emph_content],
    [$._strong_content],
    // [$.paragraph],
    // [$.paragraph, $.line],
    // [$.paragraph, $.word],
  ],
});
