#!/bin/bash

################################################################################
#
# This script installs some utilites
# tests in Ubuntu.
#
# Usage: ./install_locales.sh
#
# Requirements:
# - having superuser privileges
#
################################################################################

sudo apt-get update
sudo apt-get install libzip-dev
sudo apt-get install libxml2-dev
