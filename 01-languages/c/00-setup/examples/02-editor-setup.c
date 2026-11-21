#include <stdio.h>
#include <stdbool.h>

/**
 * @brief Represents a developer editor configuration profile.
 */
typedef struct {
    const char *editor_name;
    const char *lsp_server;
    bool has_format_on_save;
    bool has_debugger_adapter;
} EditorConfig;

/**
 * @brief Prints the configuration diagnostic of the developer environment.
 * @param config Pointer to the EditorConfig structure.
 */
static void print_editor_config(const EditorConfig *config) {
    if (config == NULL) {
        return;
    }
    printf("Editor Profile:       %s\n", config->editor_name);
    printf("Language Server:      %s (clangd)\n", config->lsp_server);
    printf("Format on Save:       %s\n", config->has_format_on_save ? "Enabled" : "Disabled");
    printf("DAP Debugger:         %s\n", config->has_debugger_adapter ? "Configured" : "Missing");
}

int main(void) {
    EditorConfig dev_setup = {
        .editor_name = "VS Code / Neovim",
        .lsp_server = "clangd v17+",
        .has_format_on_save = true,
        .has_debugger_adapter = true
    };

    printf("=== Editor & Tooling Diagnostic ===\n");
    print_editor_config(&dev_setup);
    return 0;
}
