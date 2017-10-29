#!/bin/bash

foo(){
    curl http://localhost:1234/
    curl localhost:1234/factsheet.pdf
}

for i in {1..500}; do
    foo &
done