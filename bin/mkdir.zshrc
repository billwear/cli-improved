mkdir() {
    # We inject -p right at the start. 
    # Any subsequent flags passed by the user (like -m 755) will override or complement it.
    command mkdir -p "$@"
}