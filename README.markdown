# Music Library (Micro Frontend)

A React-based micro frontend for displaying and managing a music library, integrated into the Main application via Module Federation. It provides a clean UI for browsing songs with filtering, sorting, and grouping capabilities, and supports role-based UI controls using mock JWT authentication.

## Table of Contents
- [How to Run Locally](#how-to-run-locally)
- [How It Was Deployed](#how-it-was-deployed)
- [Demo Credentials](#demo-credentials)
- [Micro Frontend Architecture](#micro-frontend-architecture)
- [Role-Based Authentication](#role-based-authentication)

## How to Run Locally

### Prerequisites
- Node.js (v18 or higher)
- npm or yarn
- Git
- Main app (from `https://github.com/harshdhoke/Main`) running locally to act as the container

### Steps
1. **Clone the Repository**:
   ```bash
   git clone https://github.com/harshdhoke/Music-Library.git
   cd Music-Library
   ```
2. **Install Dependencies**:
   ```bash
   npm install
   ```
3. **Build the Application**:
   ```bash
   npm run build
   ```
4. **Preview the Application**:
   ```bash
   npm run preview
   ```
   - The micro frontend runs on `http://localhost:5001` and exposes a `remoteEntry.js` for Module Federation.
5. **Access the Application**:
   - Ensure the Main app is running (`http://localhost:4173`).
   - The Music Library micro frontend will be loaded dynamically by the Main app.

## How It Was Deployed

The Music Library micro frontend is deployed as a static site on Netlify, designed to be consumed by the Main app.

### Deployment Steps
1. **Netlify Setup**:
   - Connected the `Music-Library` repository to Netlify via the Netlify dashboard.
   - Set build command to `npm run build` and publish directory to `dist`.
2. **Module Federation Configuration**:
   - Configured Vite to expose the micro frontend:
     ```javascript
     import federation from "@originjs/vite-plugin-federation";
     export default {
       plugins: [
         federation({
           name: "music_library",
           filename: "remoteEntry.js",
           exposes: {
             "./MusicLibrary": "./src/MusicLibrary",
           },
           shared: ["react", "react-dom"],
         }),
       ],
     };
     ```
3. **Automatic Deploys**:
   - Netlify deploys on pushes to the `main` branch.
4. **Production URL**:
   - Deployed at `https://music-library-finac-plus.netlify.app`.

## Demo Credentials

The application uses mock JWT authentication stored in localStorage. Use these credentials to test role-based features:

### Admin
- **Email**: admin
- **Password**: admin123
- **Role**: Can add and delete songs, view all UI controls.

### User
- **Email**: user
- **Password**: user123
- **Role**: Can view and filter songs, no add/delete controls.

To log in:
1. Access the Main app (`http://localhost:4173` or `https://main-finac-plus.netlify.app`).
2. Enter the credentials in the login form.
3. The mock JWT will be stored in localStorage, controlling UI visibility.

## Micro Frontend Architecture

The Music Library is a micro frontend integrated into the Main app using Vite’s Module Federation plugin.

### How It Works
- **Module Federation**:
  - The Music Library exposes a `MusicLibrary` component via `remoteEntry.js`.
  - Configuration in `vite.config.js`:
    ```javascript
    exposes: {
      "./MusicLibrary": "./src/MusicLibrary",
    },
    shared: ["react", "react-dom"],
    ```
  - The Main app dynamically imports this component at runtime.
- **Features**:
  - Displays a song list with filter, sort, and group-by options (by album, artist, title).
  - Uses JavaScript methods (`map`, `filter`, `reduce`) for data manipulation.
  - Styled with Tailwind CSS for a clean, responsive UI.
- **Integration**:
  - The Main app loads the Music Library at the `/library` route.
  - Lazy loading ensures efficient performance.
- **Benefits**:
  - Independent development and deployment from the Main app.
  - Shared dependencies (`react`, `react-dom`) reduce bundle size.

### Example Flow
1. User navigates to `/library` in the Main app.
2. Main app fetches `https://music-library-finac-plus.netlify.app/remoteEntry.js`.
3. The `MusicLibrary` component renders within the Main app’s container.

## Role-Based Authentication

The application implements mock role-based authentication using in-memory JWTs stored in localStorage, with no backend.

### How It Works
- **Authentication**:
  - A login form in the Main app accepts email and password.
  - Mock JWTs are generated in-memory (e.g., as a JSON object).
  - Example JWT payload:
    ```json
    {
      "email": "admin",
      "password": "admin123",
      "role": "admin"
    }
    ```
  - Stored in localStorage via React Context API.
- **Authorization**:
  - UI components use a `RoleGuard` to show/hide features:
    ```jsx
    import { useAuth } from './AuthContext';
    const RoleGuard = ({ roles, children }) => {
      const { user } = useAuth();
      return roles.includes(user?.role) ? children : null;
    };
    <RoleGuard roles={["admin"]}>
      <AddSongButton />
    </RoleGuard>
    ```
  - Admin role: Shows add/delete song buttons.
  - User role: Only shows view and filter options.
- **State Management**:
  - Uses `useState` and Context API to manage user state.
- **Security**:
  - Mock JWTs are not validated (no backend).
  - Credentials are hardcoded for demo purposes.

### Example Flow
1. User logs in with `admin`.
2. Mock JWT with `role: "admin"` is stored in localStorage.
3. Add/delete buttons are visible in the Music Library UI.
4. Logging in as `user` hides these buttons.