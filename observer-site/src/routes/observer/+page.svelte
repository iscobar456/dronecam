<script lang="ts">
	import type { MessageBody, Node } from '$lib/types';
	import { onMount } from 'svelte';

	onMount(() => {
		main();
	});

	const id = crypto.randomUUID();
	let ws: WebSocket;
	var pc: RTCPeerConnection;
	let offer: RTCSessionDescriptionInit | null = $state(null);
	let peerId = $state('');
	let iceQueue: Array<RTCIceCandidate> = $state([]);
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
		pc.onnegotiationneeded = handleNegotiation;
		pc.oniceconnectionstatechange = () => {
			console.log('ICE connection state: ' + pc.iceConnectionState);
		};
		pc.onconnectionstatechange = () => {
			console.log('Connection state: ' + pc.connectionState);
		};
		pc.addTransceiver('video', { direction: 'recvonly' });

		ws = new WebSocket(`ws://localhost:8080?id=${id}&type=observer`);
		ws.addEventListener('open', () => {
			console.log('connection established');
		});
		ws.addEventListener('close', () => {
			console.log('connection closed');
		});
		ws.addEventListener('message', messageHandler);
		ws.addEventListener('error', () => {});
	}

	const handleIceCandidate = (event: RTCPeerConnectionIceEvent) => {
		if (!event.candidate) {
			return;
		}
		if (peerId) {
			sendMessage(ws, createIceMessage(event.candidate));
		} else {
			iceQueue.push(event.candidate);
		}
	};

	const handleNegotiation = async () => {
		offer = await pc.createOffer();
		pc.setLocalDescription(offer);
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
			handleOffer(message.body);
		} else if (message.type === 'ice') {
			const iceCandidate = message.body.data;
			pc.addIceCandidate(iceCandidate);
		} else if (message.type === 'list') {
			const res = JSON.parse(message.body);
			drones = res;
		}
	};

	const handleOffer = async (offerMessage: MessageBody) => {
		console.log('Handling offer');
		const offer = JSON.parse(offerMessage.data);
		const offerer = offerMessage.from;
		await pc.setRemoteDescription(offer as RTCSessionDescription);
		const answer = await pc.createAnswer(offer);
		pc.setLocalDescription(answer);
		const response = JSON.stringify({
			type: 'sd',
			body: { from: id, to: offerer, data: JSON.stringify(answer) }
		});
		sendMessage(ws, response);
	};

	const connectToDrone = async (droneId: string) => {
		handleNegotiation();
		console.log(pc.connectionState);
		console.log(pc.iceConnectionState);
		peerId = droneId;
		iceQueue.forEach((candidate) => {
			sendMessage(ws, createIceMessage(candidate));
		});
		const message = {
			type: 'sd',
			body: {
				from: id,
				to: peerId,
				data: JSON.stringify(offer)
			}
		};
		sendMessage(ws, JSON.stringify(message));
		console.log('sent sd');
		console.log(message);
	};

	const sendMessage = (ws: WebSocket, message: string) => {
		ws.send(message);
	};
</script>

<div class="mx-auto flex w-md flex-col items-center">
	<button class="self-end bg-blue-400 px-3 py-2" onclick={refreshList}
		>Refresh list of drones</button
	>
	<hr class="my-4 h-px w-full bg-black" />
	<ol class="flex w-full flex-col gap-3">
		{#each drones as drone (drone.id)}
			<li class="flex w-full">
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
	<hr class="my-4 h-px w-full bg-black" />
	<p>{peerId}</p>
</div>
