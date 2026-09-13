#!/bin/bash


echo "Installing executable..."
sudo curl -o /usr/local/bin/dronecam http://files.isaacspencer.com/dronecam
sudo chmod a+x /usr/local/bin/dronecam

echo "\nInstalling service..."
sudo echo "[Unit]
Description=starts camera stream and connection manager
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/dronecam
Restart=always
RestartSec=30s

[Install]
WantedBy=multi-user.target
" > /etc/systemd/system/dronecam.service

sudo systemctl daemon-reload
sudo systemctl enable dronecam.service
sudo systemctl start dronecam.service
