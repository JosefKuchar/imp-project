import { defineConfig } from "vite";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import { compression } from "vite-plugin-compression2";

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [
    svelte(),
    compression({
      deleteOriginalAssets: true,
    }),
  ],
  build: {
    outDir: "../data",
  },
});
