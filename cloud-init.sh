#!/bin/bash
# LOB Dev Server Bootstrap Script
set -e

# Install dev tools
dnf install -y gcc-c++ cmake git openssl-devel sqlite-devel ledger curl

# Open firewall port 8080
firewall-cmd --permanent --add-port=8080/tcp || true
firewall-cmd --reload || true

# Create marker file
echo "Bootstrap complete at $(date)" > /home/opc/bootstrap-complete.txt
chown opc:opc /home/opc/bootstrap-complete.txt
