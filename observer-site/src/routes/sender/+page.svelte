<script lang="ts">
	import { onMount } from 'svelte';

	onMount(() => {
		main();
	});

	const id = crypto.randomUUID();
	const SSURL = `ws://localhost:8080?id=${id}&type=drone`;
	let offerString = $state('');
	let pc: RTCPeerConnection;
	let ws: WebSocket;
	let observerId = $state('');
	let pendingOutgoingIce: Array<RTCIceCandidate> = $state([]);
	let pendingIncomingIce: Array<RTCIceCandidate> = $state([]);

	const main = () => {
		const config = {
			iceServers: [
				{ urls: 'stun:stun.barracuda.com:3478' },
				{ urls: 'stun:stun.actionvoip.com:3478' }
			]
		};
		pc = new RTCPeerConnection(config);
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

	const handleOffer = async (remote: RTCSessionDescriptionInit) => {
		console.log('setting remote description...');
		try {
			if (pc.signalingState === 'have-local-offer') {
				await pc.setLocalDescription({ type: 'rollback' });
			}
		} catch (e) {
			console.warn('rollback before remote offer', e);
		}
		await pc.setRemoteDescription(remote);
		const answer = await pc.createAnswer();
		await pc.setLocalDescription(answer);
		offerString = answer.sdp || '';
		let remotePre = document.querySelector('#remote');
		if (remotePre && pc.remoteDescription) {
			remotePre.textContent = pc.remoteDescription.sdp;
		}
		const message = {
			type: 'sd',
			body: {
				from: id,
				to: observerId,
				data: JSON.stringify(answer)
			}
		};
		sendMessage(ws, JSON.stringify(message));
		pendingIncomingIce.forEach((candidate) => {
			console.log('adding queued candidate');
			pc.addIceCandidate(candidate);
		});
		pendingIncomingIce.length = 0;
		console.log('pc state is ' + pc.connectionState);
	};

	const handleIce = (event: RTCPeerConnectionIceEvent) => {
		if (event.candidate == null) {
			return;
		}
		if (observerId) {
			sendIceCandidate(event.candidate);
		} else {
			pendingOutgoingIce.push(event.candidate);
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
			if (!observerId) {
				observerId = offerer;
				pendingOutgoingIce.forEach((candidate: RTCIceCandidate) => {
					sendIceCandidate(candidate);
				});
				pendingOutgoingIce.length = 0;
			}
			await handleOffer(JSON.parse(message.body.data) as RTCSessionDescriptionInit);
		} else if (message.type === 'ice') {
			const iceCandidate = JSON.parse(message.body.data);
			handleReceivedIce(iceCandidate);
		} else if (message.type === 'disc') {
		}
	};

	const handleReceivedIce = (candidate: RTCIceCandidate) => {
		if (!pc.remoteDescription) {
			console.log('adding ice to back log');
			pendingIncomingIce.push(candidate);
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
