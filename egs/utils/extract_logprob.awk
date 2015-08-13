NR < 3 {
    next;
}

NF != 3 {
    exit;
} 

{
    total += log($2)
}

$3 == "</s>" {
    print total;
    total = 0.0
}
