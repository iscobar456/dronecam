<script lang="ts">
	import { onMount } from 'svelte';

	onMount(() => {
		main();
	});

	const id = crypto.randomUUID();
	const SSURL = `ws://localhost:8080?id=${id}&type=drone`;
	let offer = $state({});
	let offerString = $derived(JSON.stringify(offer));
	let pc: RTCPeerConnection;
	let ws: WebSocket;
	let observerId = $state('');
	let iceBacklog: Array<RTCIceCandidate> = $state([]);

	const main = () => {
		const config = {
			iceServers: [
				{ urls: 'stun:stun.barracuda.com:3478' },
				{ urls: 'stun:stun.actionvoip.com:3478' }
			]
		};
		pc = new RTCPeerConnection(config);
		pc.onnegotiationneeded = handleNegotiation;
		pc.onicecandidate = handleIce;
		pc.onconnectionstatechange = () => {
			console.log(pc.connectionState);
		};

		ws = new WebSocket(SSURL);
		ws.addEventListener('open', () => {
			console.log('connection established');
		});
		ws.addEventListener('close', () => {
			console.log('connection closed');
		});
		ws.addEventListener('message', messageHandler);
		ws.addEventListener('error', () => {});
	};

	const startVideo = () => {
		console.log('video');
		navigator.mediaDevices.getUserMedia({ video: true }).then((stream) => {
			const videoElement = document.querySelector('video');
			if (videoElement) {
				videoElement.srcObject = stream;
			}
			stream.getTracks().forEach((track) => {
				pc.addTrack(track, stream);
			});
		});
	};

	const updateDescription = (descr: RTCSessionDescriptionInit) => {
		pc.setLocalDescription(descr);
		if (observerId != null) {
			const message = {
				type: 'sd',
				body: {
					from: id,
					to: observerId,
					data: JSON.stringify(descr)
				}
			};
			sendMessage(ws, JSON.stringify(message));
		}
	};

	const handleNegotiation = async () => {
		console.log('track added');
		const offer = await pc.createOffer();
		offerString = offer.sdp || '';
		updateDescription(offer);
	};

	const handleOffer = async (offer: RTCSessionDescriptionInit) => {
		console.log('setting remote description...');
		await pc.setRemoteDescription(offer);
		let remotePre = document.querySelector('#remote');
		if (remotePre && pc.remoteDescription) {
			remotePre.textContent = pc.remoteDescription.sdp;
		}
		iceBacklog.forEach((candidate) => {
			console.log('adding queued candidate');
			handleReceivedIce(candidate);
		});
		console.log('pc state is ' + pc.connectionState);
	};

	const handleIce = (event: RTCPeerConnectionIceEvent) => {
		if (event.candidate == null) {
			return;
		}
		if (observerId) {
			sendIceCandidate(event.candidate);
		} else {
			iceBacklog.push(event.candidate);
		}
	};

	const sendIceCandidate = (candidate: RTCIceCandidate) => {
		console.log('sending ice candidate');
		const message = JSON.stringify({
			type: 'ice',
			body: {
				from: id,
				to: observerId,
				data: JSON.stringify(candidate)
			}
		});
		sendMessage(ws, message);
	};

	const messageHandler = async (event: MessageEvent) => {
		const message = JSON.parse(event.data);
		console.log('Received message: ' + message.type);
		if (message.type === 'sd') {
			const offerer = message.body.from;
			if (observerId == null) {
				observerId = offerer;
				iceBacklog.forEach((candidate: RTCIceCandidate) => {
					sendIceCandidate(candidate);
				});
			}
			handleOffer(JSON.parse(message.body.data) as RTCSessionDescriptionInit);
		} else if (message.type === 'ice') {
			const iceCandidate = JSON.parse(message.body.data);
			handleReceivedIce(iceCandidate);
		} else if (message.type === 'disc') {
		}
	};

	const handleReceivedIce = (candidate: RTCIceCandidate) => {
		if (!pc.remoteDescription) {
			console.log('adding ice to back log');
			iceBacklog.push(candidate);
		} else {
			console.log('adding ice candidate');
			pc.addIceCandidate(candidate);
		}
	};

	const sendMessage = (ws: WebSocket, message: string) => {
		console.log(`Sending message ${message}`);
		ws.send(message);
	};
</script>

<div class="flex justify-center">
	<div class="flex flex-col gap-4">
		<video autoplay></video>
		<button onclick={startVideo} style="padding: 10px; background-color: beige;"
			>Start video stream</button
		>
		<pre
			style="max-width: 500px; white-space: normal; max-height: 200px; overflow: scroll;">{offerString}</pre>
		<pre
			style="max-width: 500px; white-space: normal; max-height: 200px; overflow: scroll;"
			id="remote"></pre>
	</div>
</div>
