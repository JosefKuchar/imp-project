<script lang="ts">
  import { onMount } from "svelte";

  let canvas: HTMLCanvasElement;
  let ctx: CanvasRenderingContext2D;

  onMount(() => {
    ctx = canvas.getContext("2d")!;
    fetchImg();
  });

  const grayScaleToRGBAArray = (grayScale: Uint8ClampedArray) => {
    const rgb = new Uint8ClampedArray(grayScale.length * 4);
    for (let i = 0; i < grayScale.length; i++) {
      rgb[i * 4] = grayScale[i];
      rgb[i * 4 + 1] = grayScale[i];
      rgb[i * 4 + 2] = grayScale[i];
      rgb[i * 4 + 3] = 255;
    }
    return rgb;
  };

  const fetchImg = () =>
    fetch("http://10.0.0.146/image")
      .then((res) => res.arrayBuffer())
      .then((buffer) => {
        // Buffer contains raw grayscale image data, let's draw it
        const img = new ImageData(
          grayScaleToRGBAArray(new Uint8ClampedArray(buffer)),
          480,
          320
        );
        ctx.putImageData(img, 0, 0);
      });

  const captureImg = () => {
    fetch("http://10.0.0.146/get").then(() => fetchImg());
  };
</script>

<main>
  <canvas bind:this={canvas} width="480" height="320"></canvas>

  <button on:click={captureImg}>Get new image</button>
</main>
