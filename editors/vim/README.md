# Vim syntax highlighting for Tx

**Install Manually**

Copy or symlink `/path/to/tx/editors/vim` directory to the vim plugins
directory, which is either `~/.vim/pack/plugins/start/` for vim or
`~/.local/share/nvim/site/pack/plugins/start/` for neovim.

**Install with [vim-plug](https://github.com/junegunn/vim-plug)**

 1. Add `Plug 'thmxv/tx-lang', { 'rtp': 'editors/vim' }` to your .vimrc
 2. Run `:PlugInstall`

**[Neovim only] Install with [packer.nvim](https://github.com/wbthomason/packer.nvim)**

 1. Add the following to your packer startup:
    ```lua
    use { '<path/to/tx>/editors/vim', as = 'Tx' }
    ```
    Where `<path/to/tx>` is to be substituted by the location of this 
    repository in your local filesystem.

 2. Run `:PackerSync`
