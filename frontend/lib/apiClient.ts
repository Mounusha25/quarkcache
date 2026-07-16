/**
 * Axios client pre-configured for the QuarkCache FastAPI backend.
 * Next.js rewrites /api/* → http://localhost:8000/* in development.
 */

import axios from "axios";

const api = axios.create({
  baseURL: "/api",
  timeout: 30_000,
  headers: { "Content-Type": "application/json" },
});

export default api;
