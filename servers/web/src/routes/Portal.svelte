<script lang="ts">
	let { stream, disconnect } = $props();
	import ExitSvg from '$lib/assets/exit-svgrepo-com.svg';
</script>

<div
	style:display={stream == null ? 'none' : 'flex'}
	class="fixed inset-0 h-screen w-screen bg-mist-200 p-10 dark:bg-neutral-700"
>
	<video
		id="remoteVideo"
		class="w-full rounded-md bg-black"
		srcobject={stream}
		autoplay
		playsinline
		muted
	>
	</video>

	<button
		class="absolute top-0 right-0 m-1 h-8 w-8 cursor-pointer dark:invert"
		onclick={() => {
			disconnect();
		}}><img src={ExitSvg} alt="Exit icon" /></button
	>
</div>
