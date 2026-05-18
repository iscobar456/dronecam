#!/bin/bash


echo "Installing executable..."
sudo curl -o http://files.isaacspencer.com/dronecam /usr/local/bin/dronecam

echo "\nInstalling service..."
sudo echo "[Unit]
Description=starts camera stream and connection manager

[Service]
Type=simple
ExecStart=/usr/local/bin/dronecam
Restart=always
RestartSec=30s

[Install]
WantedBy=default.target
RequiredBy=network.target
" > /etc/systemd/system/dronecam.service

sudo systemctl daemon-reload
sudo systemctl enable dronecam.service
sudo systemctl start dronecam.service
