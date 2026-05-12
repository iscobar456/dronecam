<script lang="ts">
	import type { MessageBody, Node } from '$lib/types';
	import { onMount } from 'svelte';
	import DroneImage from '$lib/assets/mq9-reaper.jpg';
	import { PUBLIC_ENVIRONMENT } from '$env/static/public';

	const WEBSOCKET_URL =
		PUBLIC_ENVIRONMENT == 'dev' ? 'ws://localhost:8080' : 'wss://dcsignaling.isaacspencer.com/';

	onMount(() => {
		main();
	});

	const id = crypto.randomUUID();
	let ws: WebSocket;
	var pc: RTCPeerConnection;
	let peerId = $state('');
	let iceQueue: Array<RTCIceCandidate> = $state([]);
	let remoteIceQueue: Array<RTCIceCandidate> = $state([]);
	let drones: Array<Node> = $state([]);

	function main() {
		const config = {
			iceServers: [
				{ urls: 'stun:stun.barracuda.com:3478' },
				{ urls: 'stun:stun.actionvoip.com:3478' }
			]
		};
		pc = new RTCPeerConnection(config);
		pc.onicecandidate = handleIceCandidate;
		pc.oniceconnectionstatechange = () => {
			console.log('ICE connection state: ' + pc.iceConnectionState);
		};
		pc.onconnectionstatechange = () => {
			console.log('Connection state: ' + pc.connectionState);
		};
		pc.ontrack = (ev: RTCTrackEvent) => {
			console.log('got track');
			const el = document.querySelector('#remoteVideo') as HTMLVideoElement | null;
			if (el && ev.streams[0]) {
				el.srcObject = ev.streams[0];
			}
		};
		pc.addTransceiver('video', { direction: 'recvonly' });

		ws = new WebSocket(`${WEBSOCKET_URL}?id=${id}&type=observer`);
		ws.addEventListener('open', () => {
			// console.log('connection established');
		});
		ws.addEventListener('close', () => {
			// console.log('connection closed');
		});
		ws.addEventListener('message', messageHandler);
		ws.addEventListener('error', () => {});
	}

	const handleIceCandidate = (event: RTCPeerConnectionIceEvent) => {
		console.log('got an ice candidate');
		if (!event.candidate) {
			return;
		}
		if (event.candidate.candidate.includes(' tcp ')) return;
		if (event.candidate.candidate.includes('.local')) return;
		if (peerId) {
			sendMessage(ws, createIceMessage(event.candidate));
		} else {
			iceQueue.push(event.candidate);
		}
	};

	const createIceMessage = (candidate: RTCIceCandidate) => {
		const message = JSON.stringify({
			type: 'ice',
			body: {
				from: id,
				to: peerId,
				data: JSON.stringify(candidate)
			}
		});
		return message;
	};

	const refreshList = () => {
		const message = {
			type: 'list',
			body: {
				to: '',
				from: id,
				data: ''
			}
		};
		sendMessage(ws, JSON.stringify(message));
	};

	const messageHandler = async (event: MessageEvent) => {
		const message = JSON.parse(event.data);

		if (message.type === 'sd') {
			await handleOffer(message.body);
		} else if (message.type === 'ice') {
			handleIceMessage(message.body.data);
		} else if (message.type === 'list') {
			const res = JSON.parse(message.body);
			drones = res;
		}
	};

	const handleIceMessage = (cand: string) => {
		console.log('received an ice candidate: ' + cand);
		const candidate = new RTCIceCandidate({ candidate: cand, sdpMLineIndex: 0 });
		if (pc.remoteDescription != null) {
			pc.addIceCandidate(candidate);
		} else {
			remoteIceQueue.push(candidate);
		}
	};

	const handleOffer = async (offerMessage: MessageBody) => {
		console.log('Handling remote SDP');
		console.log(offerMessage.data);
		const sd = offerMessage.data;
		await pc.setRemoteDescription({ sdp: sd, type: 'offer' });
		const answer = await pc.createAnswer();
		await pc.setLocalDescription(answer);
		const message = {
			type: 'sd',
			body: {
				from: id,
				to: peerId,
				data: JSON.stringify(answer)
			}
		};
		sendMessage(ws, JSON.stringify(message));

		remoteIceQueue.forEach((candidate) => pc.addIceCandidate(candidate));
	};

	const connectToDrone = async (droneId: string) => {
		peerId = droneId;
		const message = {
			type: 'select',
			body: {
				from: id,
				to: peerId,
				data: ''
			}
		};
		sendMessage(ws, JSON.stringify(message));

		iceQueue.forEach((candidate) => {
			sendMessage(ws, createIceMessage(candidate));
		});
		iceQueue.length = 0;
	};

	const sendMessage = (ws: WebSocket, message: string) => {
		ws.send(message);
	};
</script>

<h2 class="mb-5 text-center text-2xl">Drones</h2>
<div class="grid w-full grid-cols-2 px-15">
	<ol class="grid w-full grid-cols-[200px] flex-col gap-3">
		<li>
			<button class="self-end bg-blue-400 px-3 py-2" onclick={refreshList}>
				Refresh list of drones
			</button>
		</li>
		{#each drones as drone (drone.id)}
			<li class="w-full gap-3 rounded-sm border">
				<img src={DroneImage} alt="drone image" />
				<p>{drone.name}</p>
				<button
					class="ml-auto bg-green-300 px-2 py-1"
					onclick={() => {
						connectToDrone(drone.id);
					}}>Connect</button
				>
			</li>
		{/each}
	</ol>
	<div>
		<p>{peerId}</p>
		<video id="remoteVideo" class="mt-4 max-h-64 w-full bg-black" autoplay playsinline muted>
		</video>
	</div>
</div>
<hr />

remote ice queue
<ol>
	{#each remoteIceQueue as cand}
		<li>{JSON.stringify(cand)}</li>
	{/each}
</ol>

local ice queue
<ol>
	{#each iceQueue as cand}
		<li>{JSON.stringify(cand)}</li>
	{/each}
</ol>
