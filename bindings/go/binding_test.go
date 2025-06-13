package tree_sitter_quarto_test

import (
	"testing"

	tree_sitter "github.com/smacker/go-tree-sitter"
	"github.com/tree-sitter/tree-sitter-quarto"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_quarto.Language())
	if language == nil {
		t.Errorf("Error loading Quarto grammar")
	}
}
