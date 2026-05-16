clear() {
    # 1. Clear the screen visually (standard behavior)
    /usr/bin/clear
    
    # 2. Wipe the scrollback buffer entirely using printf escape codes
    # \033c resets the terminal, \033[3J clears the scrollback history
    printf '\033c\033[3J'
}
alias cls='clear'